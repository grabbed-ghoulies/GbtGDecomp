#pragma once
// Host plugins: folders of extra or replacement disc files and AIDs, plus switches for compiled-in features.
// None of this exists in retail builds: the sources compile only without GBTG_RETAIL_GAME_ONLY, and every caller
// guards its call with #if !defined(GBTG_RETAIL_GAME_ONLY). See docs/PLUGINS.md.
#if !defined(GBTG_RETAIL_GAME_ONLY)
#include "gbtg/bundle.h"
#include "gbtg/plugin_api.h"

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace gbtg
{
struct Game;

struct LoadedPlugin
{
    std::string id; // The plugin's folder name.
    std::string name; // [Plugin] Name, else the id.
    std::string version; // [Plugin] Version.
    int priority = 0; // [Plugin] Priority: higher loads later, so its files win.
    std::filesystem::path dir;
    std::vector<std::string> features; // [Plugin] Features, comma separated.
    std::string library; // [Plugin] Library: a .dll / .so in the plugin's folder; no extension adds the platform's.
    std::map<std::string, std::map<std::string, std::string>> sections; // Every other section, keys lower case.
    size_t files = 0; // Disc files this plugin supplies, including its aid/ files.
};

// Reads every enabled plugin under each of `plugins_dirs`, builds the disc overlay over `game_dir` and loads the
// plugins' libraries. Run once at boot, after the language is chosen and before the game opens any disc file. A
// missing folder loads nothing; a plugin id already loaded from an earlier folder is skipped.
void load_plugins(const std::vector<std::filesystem::path>& plugins_dirs, const std::filesystem::path& game_dir,
    const std::filesystem::path& executable_dir);
const std::vector<LoadedPlugin>& loaded_plugins();

// The plugin file that replaces or adds `path` (a host path under the game folder), else `path` unchanged.
std::filesystem::path plugin_disc_path(const std::filesystem::path& path);
// The same for an Xbox path already mapped to host separators (xbox_host_path).
std::string plugin_host_path(std::string path);
// Whether a plugin's scene recipe (scenes/*.scene) makes the bundle at `path`, a host path under the game folder.
bool plugin_recipe_exists(const std::filesystem::path& path);
// Makes `recipe` (a scene recipe file) the source of its bundle, replacing any plugin's, as if a plugin shipped it.
// The inspector's scene builder registers the recipe it edits this way. Returns the bundle's path under bundles/.
std::string plugin_register_recipe(const std::filesystem::path& recipe);
// The disc scene whose scene directory row (0x00116220) a recipe's scene uses ([Bundle] Transition), or null.
const char* plugin_scene_transition(const char* scene);
// The objparams (aid_objparams_*) the registered recipes add to the executable's pools, built from `executable`'s
// own records and the disc.
class SceneExecutableResources;
std::vector<std::pair<std::string, std::vector<u8>>> plugin_executable_objparams(
    const SceneExecutableResources& executable);
// The disc bundles (relative to bundles/) a recipe's bundle opens with it ([Bundle] Fallback).
std::vector<std::string> plugin_recipe_fallbacks(const std::filesystem::path& path);
// That bundle, assembled from the disc; nullopt when no recipe makes it.
std::optional<Bundle> plugin_recipe_bundle(const std::filesystem::path& path, const std::filesystem::path& game_dir);
// Points every entry of `bundle` that a plugin's aid/ folder replaces at the plugin's loose resource.
void plugin_patch_bundle(Bundle& bundle);

// After Game::init: each enabled feature's boot hook. Each frame after frame_tick: its frame hook.
void plugins_boot(Game& game);
void plugins_frame(Game& game);
// Plugin libraries' graphics hooks: after gfx_init succeeds, before the first frame; after the last frame, before
// gfx_shutdown.
void plugins_graphics_ready(Game& game);
void plugins_graphics_shutdown(Game& game);

// Plugin stories: Features menu entries that play a list of scenes (plugin_stories.cpp, gameselect_features.cpp).
struct PluginStory
{
    std::string plugin;
    std::string name; // The label when the loctext is missing (UTF-8).
    std::string loctext; // The label's aid_loctext_ resource and key, in the game's language.
    std::string key;
    std::vector<std::string> scenes; // aid_script_ names, played in order; game select follows the last.
};
void add_plugin_story(PluginStory story);
void reset_plugin_stories();
const std::vector<PluginStory>& plugin_stories();
// Boot binds the game the labels read their loctext from.
void bind_plugin_stories(Game& game);
std::u16string plugin_story_label(const PluginStory& story);
// `key` from a plugin's aid_loctext_ resource in the game's language (0x000FF410); false when the resource (checked
// first, as a missing one is a disc error) or the key is missing.
bool plugin_loctext(Game& game, const std::string& resource, const std::string& key, std::u16string& out);

// Plugin overlays (GbtgOverlay, such as the debug UI), drawn by the gfx backends over each presented frame.
// A window message (GbtgWin32Message) or SDL_Event before the game sees it: true when an overlay took it.
bool plugins_window_event(const void* event);
// True while an overlay takes the keyboard: the game reads no keys and Escape does not quit.
bool plugins_capture_input();
// After each frame's tick and before each repeated present, with the host seconds since the last call.
void plugins_overlay_build(Game& game, float host_dt);
bool plugins_overlay_has_draws();
// D3D11: null `command`, with the back buffer bound. Vulkan: the command buffer, inside dynamic rendering.
void plugins_overlay_render(void* command);
}
#endif
