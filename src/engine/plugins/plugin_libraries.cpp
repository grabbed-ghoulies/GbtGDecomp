#if !defined(GBTG_RETAIL_GAME_ONLY)
// Plugin libraries: the .dll / .so a plugin.ini names under [Plugin] Library, and the host table
// (include/gbtg/plugin_api.h) each one receives.
#include "plugin_libraries.h"

#include "../graphics/render_profile.h"
#include "../scene/dialogue_resource_owner.h"
#include "../../gbtg/text/localtext_resources.h"
#include "gbtg/font.h"
#include "gbtg/game.h"
#include "gbtg/gfx.h"
#include "gbtg/hash.h"
#include "gbtg/language.h"
#include "gbtg/log.h"
#include "gbtg/texture.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include <algorithm>
#include <cctype>
#include <deque>
#include <memory>

namespace gbtg
{
namespace
{
struct PluginLibrary
{
    std::string id;
    std::string dir;
    std::string executable_dir;
    std::map<std::string, std::map<std::string, std::string>> sections;
    GbtgPluginHost host{};
};

// A deque, so each library's host table keeps its address while later plugins load.
std::deque<PluginLibrary> g_libraries;
std::vector<GbtgFeature> g_features;

Game& game_of(GbtgGame* game)
{
    return *reinterpret_cast<Game*>(game);
}

std::string utf8(const std::filesystem::path& path)
{
    const auto text = path.u8string();
    return std::string(text.begin(), text.end());
}

PluginLibrary& library_of(void* plugin)
{
    return *static_cast<PluginLibrary*>(plugin);
}

std::string lower_copy(const char* text)
{
    std::string out(text != nullptr ? text : "");
    for (char& c : out)
    {
        c = char(std::tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

// UTF-8 to code points; a malformed byte stands for itself.
std::u32string decode_utf8(std::string_view text)
{
    std::u32string out;
    for (size_t at = 0; at < text.size();)
    {
        const u8 lead = u8(text[at]);
        const int length = lead < 0x80 ? 1 : (lead >> 5) == 6 ? 2 : (lead >> 4) == 14 ? 3 : (lead >> 3) == 30 ? 4 : 1;
        char32_t codepoint = length == 1 ? lead : lead & (0x7F >> length);
        bool valid = at + size_t(length) <= text.size();
        for (int index = 1; valid && index < length; ++index)
        {
            const u8 next = u8(text[at + size_t(index)]);
            valid = (next & 0xC0) == 0x80;
            codepoint = (codepoint << 6) | (next & 0x3F);
        }
        out.push_back(valid ? codepoint : char32_t(lead));
        at += valid ? size_t(length) : 1;
    }
    return out;
}

std::string encode_utf8(const char16_t* text)
{
    std::string out;
    for (; *text != 0; ++text)
    {
        char32_t codepoint = *text;
        if (codepoint >= 0xD800 && codepoint < 0xDC00 && text[1] >= 0xDC00 && text[1] < 0xE000)
        {
            codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + (char32_t(text[1]) - 0xDC00);
            ++text;
        }
        if (codepoint < 0x80)
        {
            out.push_back(char(codepoint));
        }
        else if (codepoint < 0x800)
        {
            out.push_back(char(0xC0 | (codepoint >> 6)));
            out.push_back(char(0x80 | (codepoint & 0x3F)));
        }
        else if (codepoint < 0x10000)
        {
            out.push_back(char(0xE0 | (codepoint >> 12)));
            out.push_back(char(0x80 | ((codepoint >> 6) & 0x3F)));
            out.push_back(char(0x80 | (codepoint & 0x3F)));
        }
        else
        {
            out.push_back(char(0xF0 | (codepoint >> 18)));
            out.push_back(char(0x80 | ((codepoint >> 12) & 0x3F)));
            out.push_back(char(0x80 | ((codepoint >> 6) & 0x3F)));
            out.push_back(char(0x80 | (codepoint & 0x3F)));
        }
    }
    return out;
}

// `text` in the startup font's last glyph frames, on a transparent image just large enough to hold it.
RgbaImage text_image(const GloopyFont& font, std::u32string_view text)
{
    int cursor = 0;
    int left = 0;
    int right = 0;
    int height = 0;
    for (const char32_t codepoint : text)
    {
        const RgbaImage* glyph = font.image(codepoint, font.glyph_frame_count(codepoint) - 1);
        if (glyph != nullptr)
        {
            left = (std::min)(left, cursor - font.bearing(codepoint));
            right = (std::max)(right, cursor - font.bearing(codepoint) + glyph->width);
            height = (std::max)(height, glyph->height);
        }
        cursor += font.advance(codepoint);
    }

    RgbaImage image;
    image.width = right - left;
    image.height = height;
    if (image.width <= 0 || image.height <= 0)
    {
        return image;
    }
    image.bgra.resize(size_t(image.width) * size_t(image.height) * 4);

    cursor = 0;
    for (const char32_t codepoint : text)
    {
        const RgbaImage* glyph = font.image(codepoint, font.glyph_frame_count(codepoint) - 1);
        if (glyph != nullptr)
        {
            const int x0 = cursor - font.bearing(codepoint) - left;
            for (int y = 0; y < glyph->height; ++y)
            {
                for (int x = 0; x < glyph->width; ++x)
                {
                    const size_t source = (size_t(y) * size_t(glyph->width) + size_t(x)) * 4;
                    if (glyph->bgra[source + 3] == 0)
                    {
                        continue;
                    }
                    const size_t destination = (size_t(y) * size_t(image.width) + size_t(x0 + x)) * 4;
                    std::copy_n(glyph->bgra.begin() + std::ptrdiff_t(source), 4,
                        image.bgra.begin() + std::ptrdiff_t(destination));
                }
            }
        }
        cursor += font.advance(codepoint);
    }
    return image;
}

void host_log(void* plugin, int level, const char* message)
{
    const char* id = library_of(plugin).id.c_str();
    message = message != nullptr ? message : "";
    switch (level)
    {
    case GBTG_LOG_VERBOSE:
        log::verbose("plugin %s: %s", id, message);
        break;
    case GBTG_LOG_WARN:
        log::warn("plugin %s: %s", id, message);
        break;
    case GBTG_LOG_ERROR:
        log::error("plugin %s: %s", id, message);
        break;
    default:
        log::info("plugin %s: %s", id, message);
        break;
    }
}

const char* host_setting(void* plugin, const char* section, const char* key)
{
    const auto& sections = library_of(plugin).sections;
    const auto found = sections.find(lower_copy(section));
    if (found == sections.end())
    {
        return nullptr;
    }
    const auto value = found->second.find(lower_copy(key));
    return value == found->second.end() ? nullptr : value->second.c_str();
}

const char* host_plugin_dir(void* plugin)
{
    return library_of(plugin).dir.c_str();
}

const char* host_executable_dir(void* plugin)
{
    return library_of(plugin).executable_dir.c_str();
}

int host_add_feature(void* plugin, const GbtgFeature* feature)
{
    if (feature == nullptr || feature->id == nullptr)
    {
        return 0;
    }
    log::verbose("plugin %s: feature %s", library_of(plugin).id.c_str(), feature->id);
    g_features.push_back(*feature);
    return 1;
}

const char* host_language()
{
    return language_folder(g_language);
}

int host_loctext(GbtgGame* game, const char* resource, const char* key, char* out, size_t size)
{
    if (game == nullptr || resource == nullptr || key == nullptr || out == nullptr || size == 0)
    {
        return 0;
    }
    *out = '\0';
    Game& state = game_of(game);
    if (state.native_resources == nullptr || std::string_view(resource).substr(0, 12) != "aid_loctext_")
    {
        return 0;
    }
    // The native acquire treats a missing resource as a damaged disc (resource_error 2, which never returns), so a
    // plugin's missing translation is checked here first, where the dialogue resource owner would read it.
    const auto path = plugin_disc_path(state.aids.game_dir / "loctext" / language_folder(g_language)
        / aid_hash_hex(aid_hash(resource)));
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec))
    {
        log::warn("plugin loctext %s: no %s file", resource, language_folder(g_language));
        return 0;
    }
    char16_t text[0x400]{};
    if (acquire_localtext_key(resource, key, text, state.native_resources->text) == 0)
    {
        return 0;
    }
    const std::string encoded = encode_utf8(text);
    const size_t length = (std::min)(encoded.size(), size - 1);
    std::copy_n(encoded.data(), length, out);
    out[length] = '\0';
    return 1;
}

int host_has_window(GbtgGame*)
{
    return gfx_has_window() ? 1 : 0;
}

int host_show_loading_screen(GbtgGame* game, const char* text, float progress)
{
    if (!gfx_has_window() || text == nullptr)
    {
        return 0;
    }
    // The startup font loads once, and the text image is kept while the text stays the same, so each progress step
    // only redraws.
    static std::unique_ptr<GloopyFont> font;
    static std::string shown;
    static RgbaImage image;
    if (font == nullptr)
    {
        const auto path = plugin_disc_path(game_of(game).aids.game_dir / "font" / "069563d4");
        auto loaded = std::make_unique<GloopyFont>();
        if (!loaded->load(path))
        {
            log::warn("plugin loading screen: could not load startup font %s", path.string().c_str());
            return 0;
        }
        font = std::move(loaded);
    }
    if (shown != text || image.width <= 0)
    {
        shown = text;
        image = text_image(*font, decode_utf8(shown));
    }
    if (image.width <= 0 || image.height <= 0)
    {
        return 0;
    }
    gfx_show_startup_loading_screen(image, std::clamp(progress, 0.f, 1.f));
    return 1;
}

bool shaders_ahead_available()
{
    return !render_profile::reference && !render_profile::cpu_vertex;
}

int host_prepare_vertex_program(GbtgGame*, const uint32_t* words, size_t count)
{
    if (!shaders_ahead_available() || words == nullptr || count == 0)
    {
        return -1;
    }
    return gfx_native_vertex_program_supported(std::span<const u32>(words, count)) ? 1 : 0;
}

int host_prepare_pixel_definition(GbtgGame*, const uint32_t* words)
{
    if (!shaders_ahead_available() || words == nullptr)
    {
        return -1;
    }
    return gfx_prepare_native_pixel_definition(words) ? 1 : 0;
}

void host_compiled_shaders(GbtgGame*, const GbtgShaderSink* sink)
{
    if (sink == nullptr)
    {
        return;
    }
    std::vector<std::vector<u32>> vertex_programs;
    std::vector<std::array<u32, 60>> pixel_definitions;
    gfx_compiled_shader_permutations(vertex_programs, pixel_definitions);
    for (const auto& program : vertex_programs)
    {
        if (sink->vertex_program != nullptr)
        {
            sink->vertex_program(sink->context, program.data(), program.size());
        }
    }
    for (const auto& definition : pixel_definitions)
    {
        if (sink->pixel_definition != nullptr)
        {
            sink->pixel_definition(sink->context, definition.data());
        }
    }
}

const char* host_scene_name(const GbtgGame* game)
{
    return reinterpret_cast<const Game*>(game)->scene_script.name.c_str();
}

uint32_t host_frame(const GbtgGame* game)
{
    return reinterpret_cast<const Game*>(game)->frame;
}

std::filesystem::path library_file(const LoadedPlugin& plugin)
{
    std::filesystem::path file = plugin.dir / std::u8string(plugin.library.begin(), plugin.library.end());
    if (!file.has_extension())
    {
#if defined(_WIN32)
        file += ".dll";
#else
        file += ".so";
#endif
    }
    return file;
}

GbtgPluginLoad open_library(const std::filesystem::path& file, std::string& error)
{
#if defined(_WIN32)
    // The altered search path finds the library's own dependencies in its folder first.
    HMODULE module = LoadLibraryExW(file.wstring().c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (module == nullptr)
    {
        error = "LoadLibrary failed, error " + std::to_string(GetLastError());
        return nullptr;
    }
    auto load = reinterpret_cast<GbtgPluginLoad>(GetProcAddress(module, GBTG_PLUGIN_LOAD_SYMBOL));
#else
    void* module = dlopen(file.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (module == nullptr)
    {
        const char* reason = dlerror();
        error = reason != nullptr ? reason : "dlopen failed";
        return nullptr;
    }
    auto load = reinterpret_cast<GbtgPluginLoad>(dlsym(module, GBTG_PLUGIN_LOAD_SYMBOL));
#endif
    if (load == nullptr)
    {
        error = "no " GBTG_PLUGIN_LOAD_SYMBOL " export";
    }
    return load;
}

template <class Hook>
void run_hooks(Hook GbtgFeature::*hook, Game& game)
{
    for (const GbtgFeature& feature : g_features)
    {
        if (feature.*hook != nullptr)
        {
            (feature.*hook)(feature.user, reinterpret_cast<GbtgGame*>(&game));
        }
    }
}
}

bool load_plugin_library(const LoadedPlugin& plugin, const std::filesystem::path& executable_dir)
{
    const std::filesystem::path file = library_file(plugin);
    std::string error;
    const GbtgPluginLoad load = open_library(file, error);
    if (load == nullptr)
    {
        log::warn("plugin %s: %s: %s", plugin.id.c_str(), file.string().c_str(), error.c_str());
        return false;
    }
    PluginLibrary& library = g_libraries.emplace_back();
    library.id = plugin.id;
    library.dir = utf8(plugin.dir);
    library.executable_dir = utf8(executable_dir);
    library.sections = plugin.sections;
    GbtgPluginHost& host = library.host;
    host.api_version = GBTG_PLUGIN_API_VERSION;
    host.size = sizeof(GbtgPluginHost);
    host.plugin = &library;
    host.log = host_log;
    host.setting = host_setting;
    host.plugin_dir = host_plugin_dir;
    host.executable_dir = host_executable_dir;
    host.add_feature = host_add_feature;
    host.language = host_language;
    host.loctext = host_loctext;
    host.has_window = host_has_window;
    host.show_loading_screen = host_show_loading_screen;
    host.prepare_vertex_program = host_prepare_vertex_program;
    host.prepare_pixel_definition = host_prepare_pixel_definition;
    host.compiled_shaders = host_compiled_shaders;
    host.scene_name = host_scene_name;
    host.frame = host_frame;
    fill_overlay_calls(host);
    fill_diagnostics_calls(host);
    if (load(&host) == 0)
    {
        log::warn("plugin %s: %s declined to load", plugin.id.c_str(), file.filename().string().c_str());
        return false;
    }
    log::info("plugin %s: loaded %s", plugin.id.c_str(), file.filename().string().c_str());
    return true;
}

void reset_plugin_libraries()
{
    // Loaded libraries stay mapped; only what they registered is dropped.
    g_features.clear();
    reset_plugin_overlays();
}

const char* plugin_library_id(void* plugin)
{
    return library_of(plugin).id.c_str();
}

void plugin_libraries_boot(Game& game)
{
    for (const GbtgFeature& feature : g_features)
    {
        log::info("plugin feature %s: %s", feature.id, feature.summary != nullptr ? feature.summary : "");
    }
    run_hooks(&GbtgFeature::boot, game);
}

void plugin_libraries_frame(Game& game)
{
    run_hooks(&GbtgFeature::frame, game);
}

void plugin_libraries_graphics_ready(Game& game)
{
    run_hooks(&GbtgFeature::graphics_ready, game);
}

void plugin_libraries_graphics_shutdown(Game& game)
{
    // Overlays release their device objects here; the render thread must be done drawing them.
    plugin_overlays_wait_presented();
    run_hooks(&GbtgFeature::graphics_shutdown, game);
}
}
#endif
