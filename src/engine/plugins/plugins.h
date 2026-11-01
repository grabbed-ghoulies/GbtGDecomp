#pragma once
// Host plugins: folders of extra or replacement disc files and AIDs, plus switches for compiled-in features.
// None of this exists in retail builds: the sources compile only without GBTG_RETAIL_GAME_ONLY, and every caller
// guards its call with #if !defined(GBTG_RETAIL_GAME_ONLY). See docs/PLUGINS.md.
#if !defined(GBTG_RETAIL_GAME_ONLY)
#include "gbtg/bundle.h"
#include "gbtg/plugin_api.h"

#include <filesystem>
#include <map>
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
// Points every entry of `bundle` that a plugin's aid/ folder replaces at the plugin's loose resource.
void plugin_patch_bundle(Bundle& bundle);

// After Game::init: each enabled feature's boot hook. Each frame after frame_tick: its frame hook.
void plugins_boot(Game& game);
void plugins_frame(Game& game);
// Plugin libraries' graphics hooks: after gfx_init succeeds, before the first frame; after the last frame, before
// gfx_shutdown.
void plugins_graphics_ready(Game& game);
void plugins_graphics_shutdown(Game& game);

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
