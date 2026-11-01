// Boot shader warm-up, which retail does not have: before the first frame, compile the vertex programs and pixel
// definitions earlier runs drew (cache/shaders.gbtg) behind a "Compiling Shaders" screen, so the first frames do
// not stall on shader compiles; at exit, add this run's to the file. The screen's text is this plugin's own
// loctext, aid_loctext_plugin_shaderwarmup, in the game's language.
#include "shader_manifest.h"

#include "gbtg/plugin_api.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <exception>
#include <string>

namespace
{
const GbtgPluginHost* g_host = nullptr;
std::filesystem::path g_manifest = "cache/shaders.gbtg";

constexpr const char* kLoctext = "aid_loctext_plugin_shaderwarmup";
constexpr const char* kCompilingKey = "shaderwarmup__CompilingShaders";
constexpr const char* kCompilingFallback = "Compiling Shaders";

void say(int level, const std::string& message)
{
    g_host->log(g_host->plugin, level, message.c_str());
}

std::string compiling_text(GbtgGame* game)
{
    char text[256];
    if (g_host->loctext(game, kLoctext, kCompilingKey, text, sizeof(text)) != 0 && text[0] != '\0')
    {
        return text;
    }
    say(GBTG_LOG_WARN, std::string("no ") + kCompilingKey + " for " + g_host->language() + ", using English");
    return kCompilingFallback;
}

void warm_up(GbtgGame* game, const std::string& text)
{
    shader_warmup::ShaderPermutationManifest manifest;
    shader_warmup::read_shader_permutation_manifest(g_manifest, manifest);
    const size_t total = manifest.vertex_programs.size() + manifest.pixel_definitions.size();
    size_t completed = 0;
    size_t unsupported = 0;
    size_t pixel_fallbacks = 0;
    const auto advance = [&]()
    {
        ++completed;
        g_host->show_loading_screen(game, text.c_str(), float(completed) / float(total));
    };
    g_host->show_loading_screen(game, text.c_str(), total == 0 ? 1.f : 0.f);
    const auto start = std::chrono::steady_clock::now();
    for (const auto& program : manifest.vertex_programs)
    {
        const int prepared = g_host->prepare_vertex_program(game, program.data(), program.size());
        if (prepared < 0)
        {
            say(GBTG_LOG_INFO, "shader warm-up: shaders cannot be compiled ahead in this run");
            return;
        }
        unsupported += prepared == 0 ? 1 : 0;
        advance();
    }
    for (const auto& definition : manifest.pixel_definitions)
    {
        const int prepared = g_host->prepare_pixel_definition(game, definition.data());
        if (prepared < 0)
        {
            say(GBTG_LOG_INFO, "shader warm-up: shaders cannot be compiled ahead in this run");
            return;
        }
        pixel_fallbacks += prepared == 0 ? 1 : 0;
        advance();
    }
    const double elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    char line[256];
    std::snprintf(line, sizeof(line), "shader warm-up: %zu vertex, %zu pixel permutations in %g ms",
        manifest.vertex_programs.size(), manifest.pixel_definitions.size(), elapsed);
    std::string message = line;
    if (pixel_fallbacks != 0)
    {
        message += " (" + std::to_string(pixel_fallbacks) + " generic pixel fallbacks)";
    }
    if (unsupported != 0)
    {
        message += " (" + std::to_string(unsupported) + " unsupported vertex programs)";
    }
    say(GBTG_LOG_INFO, message);
}

void graphics_ready(void*, GbtgGame* game)
{
    if (g_host->has_window(game) == 0)
    {
        return;
    }
    // No exception may cross the C boundary: a damaged manifest is logged and the game compiles as it draws.
    try
    {
        const std::string text = compiling_text(game);
        g_host->show_loading_screen(game, text.c_str(), 0.f);
        std::error_code ec;
        if (std::filesystem::exists(g_manifest, ec))
        {
            warm_up(game, text);
        }
    }
    catch (const std::exception& error)
    {
        say(GBTG_LOG_ERROR, std::string("shader warm-up: ") + error.what());
    }
}

void graphics_shutdown(void*, GbtgGame* game)
{
    try
    {
        shader_warmup::ShaderPermutationManifest manifest;
        std::error_code ec;
        if (std::filesystem::exists(g_manifest, ec))
        {
            try
            {
                shader_warmup::read_shader_permutation_manifest(g_manifest, manifest);
            }
            catch (const std::exception& error)
            {
                say(GBTG_LOG_WARN, std::string("shader capture: replacing ") + error.what());
                manifest = {};
            }
        }
        GbtgShaderSink sink{};
        sink.context = &manifest;
        sink.vertex_program = [](void* context, const uint32_t* words, size_t count)
        {
            auto& programs = static_cast<shader_warmup::ShaderPermutationManifest*>(context)->vertex_programs;
            std::vector<uint32_t> program(words, words + count);
            if (!shader_warmup::contains_program(programs, program))
            {
                programs.push_back(std::move(program));
            }
        };
        sink.pixel_definition = [](void* context, const uint32_t* words)
        {
            auto& definitions = static_cast<shader_warmup::ShaderPermutationManifest*>(context)->pixel_definitions;
            std::array<uint32_t, 60> definition{};
            std::copy_n(words, definition.size(), definition.begin());
            if (std::find(definitions.begin(), definitions.end(), definition) == definitions.end())
            {
                definitions.push_back(definition);
            }
        };
        g_host->compiled_shaders(game, &sink);
        shader_warmup::write_shader_permutation_manifest(g_manifest, manifest);
        say(GBTG_LOG_INFO, "shader capture: " + std::to_string(manifest.vertex_programs.size()) + " vertex, "
            + std::to_string(manifest.pixel_definitions.size()) + " pixel permutations -> " + g_manifest.string());
    }
    catch (const std::exception& error)
    {
        say(GBTG_LOG_ERROR, std::string("shader capture: ") + error.what());
    }
}
}

extern "C" GBTG_PLUGIN_EXPORT int gbtg_plugin_load(const GbtgPluginHost* host)
{
    if (host->api_version != GBTG_PLUGIN_API_VERSION)
    {
        return 0;
    }
    g_host = host;
    if (const char* manifest = host->setting(host->plugin, "ShaderWarmup", "Manifest"))
    {
        const std::string_view text(manifest);
        g_manifest = std::filesystem::path(std::u8string(text.begin(), text.end()));
    }
    static const GbtgFeature feature{"shader_warmup",
        "compiles earlier runs' shaders before the first frame and records this run's at exit", nullptr, nullptr,
        graphics_ready, graphics_shutdown, nullptr};
    return host->add_feature(host->plugin, &feature);
}
