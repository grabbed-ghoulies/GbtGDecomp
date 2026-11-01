#pragma once

#include <filesystem>
#include <string>

namespace gbtg
{

// PC host settings from gbtg.ini. Reconstructed game code does not read this. Linux defaults suit the Steam Deck:
// its 1280x800 panel, fullscreen, and the Xbox's 16:9 setting.
struct HostConfig
{
#if defined(_WIN32)
    int width = 960;
    int height = 540;
    bool fullscreen = false;
    bool widescreen = false; // [Video] Widescreen: the Xbox's 16:9 video setting (XGetVideoFlags' widescreen bit).
#else
    int width = 1280;
    int height = 800;
    bool fullscreen = true; // [Video] Fullscreen: the SDL window covers the display (Linux only).
    bool widescreen = true;
#endif
    int vsync = 1;
    bool render_thread = true; // [Video] RenderThread: GBTG_OPTIMIZE builds run the gfx backend on its own thread.
    std::string gfx_backend; // [Video] Backend: d3d11, vulkan or null. Empty uses the build's default.
    int master_volume = 100; // 0..100
    unsigned language = 0; // DAT_0042A710 index, see gbtg/language.h
    std::filesystem::path game_dir; // [Game] GameDir: the disc dump. Empty uses --game-dir, else OriginalFiles.
};

// In the host data directory, then the working directory. Writes a default in the
// data directory when neither file exists.
HostConfig load_host_config(const std::filesystem::path& argv0);

// Absolute path of the running executable; empty when it cannot be resolved.
std::filesystem::path executable_path(const std::filesystem::path& argv0);
// The executable's directory, else the working directory.
std::filesystem::path host_exe_dir(const std::filesystem::path& argv0);
// Where gbtg.ini, gbtg.log, saves/ and cache/ live. Windows: the executable's directory (cache/ follows the working
// directory). Elsewhere: $GBTG_DATA_DIR, else $XDG_DATA_HOME/gbtg, else ~/.local/share/gbtg, created on first use.
std::filesystem::path host_data_dir(const std::filesystem::path& argv0);

} // namespace gbtg
