#if !defined(GBTG_RETAIL_GAME_ONLY)
#include "plugins.h"
#include "plugin_features.h"
#include "plugin_libraries.h"
#include "scene_recipe.h"

#include "gbtg/hash.h"
#include "gbtg/language.h"
#include "gbtg/log.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <unordered_map>

namespace gbtg
{
namespace
{
std::vector<LoadedPlugin> g_plugins;
std::string g_game_key; // The game folder as an overlay key prefix: absolute, '/' separated, lower case.
std::filesystem::path g_game_dir;
std::unordered_map<std::string, std::filesystem::path> g_overlay; // Disc-relative key -> plugin file.
std::unordered_map<std::string, std::filesystem::path> g_aid_files; // AID name -> plugin aid/ file.
std::map<std::string, LooseResource> g_aid_resources; // Parsed aid/ files that replaced bundle entries.
std::unordered_map<std::string, std::filesystem::path> g_recipes; // Disc-relative bundle key -> scenes/ recipe.
std::unordered_map<std::string, std::string> g_transitions; // A recipe's scene -> the disc scene it transitions as.
struct ActiveFeature
{
    const PluginFeature* feature;
    const PluginSettings* settings;
};
std::vector<ActiveFeature> g_active;

std::string lower_copy(std::string_view text)
{
    std::string out(text);
    for (char& c : out)
    {
        c = char(std::tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

std::string_view trim(std::string_view text)
{
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())))
    {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())))
    {
        text.remove_suffix(1);
    }
    return text;
}

// The overlay key for a host path: absolute, normalized, '/' separated and lower case (the Xbox's file system
// ignores case).
std::string path_key(const std::filesystem::path& path)
{
    std::error_code ec;
    std::filesystem::path full = std::filesystem::absolute(path, ec);
    if (ec)
    {
        full = path;
    }
    std::string key = lower_copy(full.lexically_normal().generic_string());
    while (key.size() > 1 && key.back() == '/')
    {
        key.pop_back();
    }
    return key;
}

using Sections = std::map<std::string, std::map<std::string, std::string>>;

Sections read_manifest(const std::filesystem::path& path)
{
    Sections sections;
    std::ifstream file(path);
    std::string section;
    std::string line;
    while (std::getline(file, line))
    {
        // Windows editors may start the file with a UTF-8 byte order mark.
        if (line.starts_with("\xEF\xBB\xBF"))
        {
            line.erase(0, 3);
        }
        std::string_view text = trim(line);
        if (text.empty() || text.front() == ';' || text.front() == '#')
        {
            continue;
        }
        if (text.front() == '[' && text.back() == ']')
        {
            section = lower_copy(trim(text.substr(1, text.size() - 2)));
            continue;
        }
        const auto eq = text.find('=');
        if (eq == std::string_view::npos)
        {
            continue;
        }
        sections[section][lower_copy(trim(text.substr(0, eq)))] = std::string(trim(text.substr(eq + 1)));
    }
    return sections;
}

std::string setting(const Sections& sections, const char* section, const char* key, const char* fallback = "")
{
    const auto found = sections.find(section);
    if (found == sections.end())
    {
        return fallback;
    }
    const auto value = found->second.find(key);
    return value == found->second.end() ? std::string(fallback) : value->second;
}

std::vector<std::string> split_list(std::string_view text)
{
    std::vector<std::string> items;
    while (!text.empty())
    {
        const auto comma = text.find(',');
        const std::string_view item = trim(text.substr(0, comma));
        if (!item.empty())
        {
            items.push_back(lower_copy(item));
        }
        if (comma == std::string_view::npos)
        {
            break;
        }
        text.remove_prefix(comma + 1);
    }
    return items;
}

// Where the game looks for a loose AID: loctext/<language>/ for text, font/ for fonts, demand/ for the rest.
std::string aid_disc_relative(const std::string& aid)
{
    const std::string hash = aid_hash_hex(aid_hash(aid));
    if (aid.starts_with("aid_loctext_"))
    {
        return std::string("loctext/") + language_folder(g_language) + "/" + hash;
    }
    if (aid.starts_with("aid_font_"))
    {
        return "font/" + hash;
    }
    return "demand/" + hash;
}

bool hidden(const std::filesystem::path& path)
{
    const std::string name = path.filename().string();
    return !name.empty() && name.front() == '.';
}

// The recipe's scene (its [Script] resource) and its Transition.
void note_recipe_transition(const SceneRecipe& recipe)
{
    for (const RecipeResource& resource : recipe.resources)
    {
        if (resource.kind != RecipeResource::Kind::Script)
        {
            continue;
        }
        if (recipe.transition.empty())
        {
            g_transitions.erase(lower_copy(resource.aid));
        }
        else
        {
            g_transitions[lower_copy(resource.aid)] = recipe.transition;
        }
    }
}

void index_plugin_files(LoadedPlugin& plugin)
{
    std::error_code ec;
    std::vector<std::pair<std::string, std::filesystem::path>> recipes;
    for (auto it = std::filesystem::recursive_directory_iterator(plugin.dir, ec);
        !ec && it != std::filesystem::recursive_directory_iterator(); it.increment(ec))
    {
        if (hidden(it->path()))
        {
            if (it->is_directory(ec))
            {
                it.disable_recursion_pending();
            }
            continue;
        }
        if (!it->is_regular_file(ec) || it.depth() == 0)
        {
            continue; // Files beside plugin.ini are the plugin's own, not disc files.
        }
        const std::filesystem::path relative = it->path().lexically_relative(plugin.dir);
        const std::string top = lower_copy(relative.begin()->string());
        std::string key;
        if (top == "aid")
        {
            const std::string aid = lower_copy(it->path().filename().string());
            if (!aid.starts_with("aid_"))
            {
                log::warn("plugin %s: %s is not named aid_..., skipped", plugin.id.c_str(),
                    relative.generic_string().c_str());
                continue;
            }
            g_aid_files[aid] = it->path();
            key = aid_disc_relative(aid);
        }
        else if (top == "scenes")
        {
            // A scene recipe makes the bundle its [Bundle] Path names, from the disc, when the game opens it.
            try
            {
                const SceneRecipe recipe = read_scene_recipe(it->path());
                recipes.emplace_back(lower_copy("bundles/" + recipe.bundle), it->path());
                note_recipe_transition(recipe);
            }
            catch (const std::exception& error)
            {
                log::warn("plugin %s: %s", plugin.id.c_str(), error.what());
            }
            continue;
        }
        else
        {
            key = lower_copy(relative.generic_string());
        }
        if (const auto previous = g_overlay.find(key); previous != g_overlay.end())
        {
            log::verbose("plugin %s: %s replaces %s", plugin.id.c_str(), key.c_str(),
                previous->second.string().c_str());
        }
        g_overlay[key] = it->path();
        g_recipes.erase(key);
        ++plugin.files;
    }
    // A bundle file the plugin ships itself (a built recipe) wins over its recipe.
    for (const auto& [key, path] : recipes)
    {
        if (const auto file = g_overlay.find(key); file != g_overlay.end() && file->second.string().starts_with(
            plugin.dir.string()))
        {
            continue;
        }
        g_overlay.erase(key);
        g_recipes[key] = path;
        ++plugin.files;
        log::verbose("plugin %s: %s from recipe %s", plugin.id.c_str(), key.c_str(), path.string().c_str());
    }
}
}

void load_plugins(const std::vector<std::filesystem::path>& plugins_dirs, const std::filesystem::path& game_dir,
    const std::filesystem::path& executable_dir)
{
    g_plugins.clear();
    g_overlay.clear();
    g_aid_files.clear();
    g_aid_resources.clear();
    g_recipes.clear();
    g_transitions.clear();
    g_active.clear();
    reset_plugin_libraries();
    reset_plugin_stories();
    g_game_key = path_key(game_dir);
    g_game_dir = game_dir;

    std::vector<std::filesystem::directory_entry> folders;
    std::vector<std::string> seen_dirs;
    for (const std::filesystem::path& plugins_dir : plugins_dirs)
    {
        std::error_code ec;
        const std::string dir_key = path_key(plugins_dir);
        if (std::find(seen_dirs.begin(), seen_dirs.end(), dir_key) != seen_dirs.end())
        {
            continue;
        }
        seen_dirs.push_back(dir_key);
        if (!std::filesystem::is_directory(plugins_dir, ec))
        {
            log::verbose("plugins: no folder %s", plugins_dir.string().c_str());
            continue;
        }
        for (const auto& entry : std::filesystem::directory_iterator(plugins_dir, ec))
        {
            folders.push_back(entry);
        }
    }
    for (const auto& entry : folders)
    {
        std::error_code ec;
        if (!entry.is_directory(ec) || hidden(entry.path()))
        {
            continue;
        }
        const std::string id = entry.path().filename().string();
        if (std::any_of(g_plugins.begin(), g_plugins.end(), [&](const LoadedPlugin& loaded)
        {
            return loaded.id == id;
        }))
        {
            log::warn("plugin %s: already loaded, %s skipped", id.c_str(), entry.path().string().c_str());
            continue;
        }
        const auto manifest = entry.path() / "plugin.ini";
        if (!std::filesystem::is_regular_file(manifest, ec))
        {
            log::warn("plugin %s: no plugin.ini, skipped", entry.path().filename().string().c_str());
            continue;
        }
        const Sections sections = read_manifest(manifest);
        LoadedPlugin plugin;
        plugin.id = entry.path().filename().string();
        if (setting(sections, "plugin", "enabled", "1") == "0")
        {
            log::info("plugin %s: disabled", plugin.id.c_str());
            continue;
        }
        plugin.name = setting(sections, "plugin", "name", plugin.id.c_str());
        plugin.version = setting(sections, "plugin", "version");
        plugin.dir = entry.path();
        plugin.features = split_list(setting(sections, "plugin", "features"));
        plugin.library = setting(sections, "plugin", "library");
        try
        {
            plugin.priority = std::stoi(setting(sections, "plugin", "priority", "0"));
        }
        catch (const std::exception&)
        {
            log::warn("plugin %s: Priority is not a number, using 0", plugin.id.c_str());
        }
        plugin.sections = sections;
        plugin.sections.erase("plugin");
        g_plugins.push_back(std::move(plugin));
    }
    // Lower priority first, so a higher priority's files replace them; ties load by folder name.
    std::sort(g_plugins.begin(), g_plugins.end(), [](const LoadedPlugin& a, const LoadedPlugin& b)
    {
        return a.priority != b.priority ? a.priority < b.priority : a.id < b.id;
    });
    for (LoadedPlugin& plugin : g_plugins)
    {
        index_plugin_files(plugin);
        for (const std::string& id : plugin.features)
        {
            const PluginFeature* feature = find_plugin_feature(id);
            if (feature == nullptr)
            {
                log::warn("plugin %s: unknown feature %s", plugin.id.c_str(), id.c_str());
                continue;
            }
            static const PluginSettings kNoSettings;
            const auto section = plugin.sections.find(id);
            const PluginSettings* settings = section == plugin.sections.end() ? &kNoSettings : &section->second;
            // A feature runs once; a later plugin that also lists it supplies its settings.
            const auto active = std::find_if(g_active.begin(), g_active.end(), [&](const ActiveFeature& item)
            {
                return item.feature == feature;
            });
            if (active != g_active.end())
            {
                active->settings = settings;
            }
            else
            {
                g_active.push_back({feature, settings});
            }
        }
        std::string features;
        for (const std::string& id : plugin.features)
        {
            features += features.empty() ? id : ", " + id;
        }
        log::info("plugin %s: %s %s, %zu files, features [%s]", plugin.id.c_str(), plugin.name.c_str(),
            plugin.version.c_str(), plugin.files, features.c_str());
        // [Story], [Story2], ...: Features menu entries (plugin_stories.cpp).
        for (const auto& [section, values] : plugin.sections)
        {
            if (!section.starts_with("story"))
            {
                continue;
            }
            PluginStory story;
            story.plugin = plugin.id;
            const auto value = [&values](const char* key)
            {
                const auto found = values.find(key);
                return found == values.end() ? std::string() : found->second;
            };
            story.name = value("name").empty() ? plugin.name : value("name");
            story.loctext = lower_copy(value("loctext"));
            story.key = value("key");
            story.scenes = split_list(value("scenes"));
            add_plugin_story(std::move(story));
        }
        if (!plugin.library.empty())
        {
            load_plugin_library(plugin, executable_dir);
        }
    }
}

const std::vector<LoadedPlugin>& loaded_plugins()
{
    return g_plugins;
}

std::filesystem::path plugin_disc_path(const std::filesystem::path& path)
{
    if (g_overlay.empty())
    {
        return path;
    }
    const std::string key = path_key(path);
    if (key.size() <= g_game_key.size() || key.compare(0, g_game_key.size(), g_game_key) != 0 ||
        key[g_game_key.size()] != '/')
    {
        return path;
    }
    const auto found = g_overlay.find(key.substr(g_game_key.size() + 1));
    if (found == g_overlay.end())
    {
        return path;
    }
    log::verbose("plugin file %s", found->second.string().c_str());
    return found->second;
}

std::string plugin_host_path(std::string path)
{
    if (g_overlay.empty() || path.empty())
    {
        return path;
    }
    const std::filesystem::path mapped = plugin_disc_path(std::filesystem::path(path));
    return mapped.string() == path ? path : mapped.string();
}

std::vector<std::pair<std::string, std::vector<u8>>> plugin_executable_objparams(
    const SceneExecutableResources& executable)
{
    std::vector<std::pair<std::string, std::vector<u8>>> out;
    if (g_recipes.empty())
    {
        return out;
    }
    const Bundle objparams = executable_objparams_bundle(executable);
    std::map<std::string, Bundle> sources;
    const RecipeBundles open = [&](const std::string& relative) -> const Bundle&
    {
        const std::string key = lower_copy(relative);
        if (key == kExecutableSource)
        {
            return objparams;
        }
        auto found = sources.find(key);
        if (found == sources.end())
        {
            found = sources.emplace(key, load_bundle(plugin_disc_path(g_game_dir / "bundles" /
                std::filesystem::path(relative)))).first;
        }
        return found->second;
    };
    for (const auto& [key, file] : g_recipes)
    {
        try
        {
            const SceneRecipe recipe = read_scene_recipe(file);
            for (const RecipeResource& resource : recipe.resources)
            {
                if (recipe_objparams(resource))
                {
                    ResourceType type{};
                    out.emplace_back(lower_copy(resource.aid), resolve_recipe_resource(resource, open, type));
                    log::verbose("plugin objparams %s from %s", resource.aid.c_str(), file.string().c_str());
                }
            }
        }
        catch (const std::exception& error)
        {
            log::warn("plugin objparams from %s: %s", file.string().c_str(), error.what());
        }
    }
    return out;
}

const char* plugin_scene_transition(const char* scene)
{
    if (g_transitions.empty() || scene == nullptr)
    {
        return nullptr;
    }
    const auto found = g_transitions.find(lower_copy(scene));
    return found == g_transitions.end() ? nullptr : found->second.c_str();
}

std::string plugin_register_recipe(const std::filesystem::path& recipe)
{
    const SceneRecipe parsed = read_scene_recipe(recipe);
    note_recipe_transition(parsed);
    const std::string key = lower_copy("bundles/" + parsed.bundle);
    g_overlay.erase(key);
    g_recipes[key] = recipe;
    log::verbose("scene recipe %s makes %s", recipe.string().c_str(), key.c_str());
    return parsed.bundle;
}

bool plugin_recipe_exists(const std::filesystem::path& path)
{
    if (g_recipes.empty())
    {
        return false;
    }
    const std::string key = path_key(path);
    return key.size() > g_game_key.size() && key.compare(0, g_game_key.size(), g_game_key) == 0 &&
        g_recipes.contains(key.substr(g_game_key.size() + 1));
}

std::vector<std::string> plugin_recipe_fallbacks(const std::filesystem::path& path)
{
    if (!plugin_recipe_exists(path))
    {
        return {};
    }
    return read_scene_recipe(g_recipes.at(path_key(path).substr(g_game_key.size() + 1))).fallbacks;
}

std::optional<Bundle> plugin_recipe_bundle(const std::filesystem::path& path, const std::filesystem::path& game_dir)
{
    if (!plugin_recipe_exists(path))
    {
        return std::nullopt;
    }
    const std::filesystem::path& file = g_recipes.at(path_key(path).substr(g_game_key.size() + 1));
    const SceneRecipe recipe = read_scene_recipe(file);
    // Sources are disc bundles, or bundle files another plugin ships; never recipes, so a recipe may remake the
    // retail bundle of its own name.
    std::map<std::string, Bundle> sources;
    const RecipeBundles open = [&](const std::string& relative) -> const Bundle&
    {
        const std::string key = lower_copy(relative);
        auto found = sources.find(key);
        if (found == sources.end())
        {
            const std::filesystem::path source = game_dir / "bundles" / std::filesystem::path(relative);
            found = sources.emplace(key, load_bundle(plugin_disc_path(source))).first;
        }
        return found->second;
    };
    Bundle bundle = build_scene_recipe(recipe, open);
    bundle.path = path.string();
    log::info("plugin scene recipe %s: %u resources from %zu bundles", file.filename().string().c_str(),
        unsigned(bundle.file_count), sources.size());
    return bundle;
}

void plugin_patch_bundle(Bundle& bundle)
{
    if (g_aid_files.empty())
    {
        return;
    }
    for (BundleEntry& entry : bundle.entries)
    {
        const std::string key = lower_copy(entry.name);
        const auto file = g_aid_files.find(key);
        if (file == g_aid_files.end())
        {
            continue;
        }
        auto resource = g_aid_resources.find(key);
        if (resource == g_aid_resources.end())
        {
            resource = g_aid_resources.emplace(key, parse_loose_resource(read_file(file->second))).first;
            log::verbose("plugin aid %s from %s", key.c_str(), file->second.string().c_str());
        }
        const LooseResource& loose = resource->second;
        entry.type = loose.header.type;
        entry.header = loose.header;
        entry.gpu_chunks = loose.gpu_chunks;
        entry.cpu = loose.cpu;
        entry.gpu = loose.gpu;
    }
}

void plugins_boot(Game& game)
{
    bind_plugin_stories(game);
    for (const ActiveFeature& active : g_active)
    {
        log::info("plugin feature %s: %s", active.feature->id, active.feature->summary);
        if (active.feature->boot != nullptr)
        {
            active.feature->boot(game, *active.settings);
        }
    }
    plugin_libraries_boot(game);
}

void plugins_frame(Game& game)
{
    for (const ActiveFeature& active : g_active)
    {
        if (active.feature->frame != nullptr)
        {
            active.feature->frame(game);
        }
    }
    plugin_libraries_frame(game);
}

void plugins_graphics_ready(Game& game)
{
    plugin_libraries_graphics_ready(game);
}

void plugins_graphics_shutdown(Game& game)
{
    plugin_libraries_graphics_shutdown(game);
}
}
#endif
