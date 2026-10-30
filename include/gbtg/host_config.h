#pragma once

#include <filesystem>
#include <string>

namespace gbtg
{

// PC host settings from gbtg.ini. Reconstructed game code does not read this.
struct HostConfig
{
    int width = 960;
    int height = 540;
    int vsync = 1;
    std::string gfx_backend; // [Video] Backend: d3d11, vulkan or null. Empty uses the build's default.
    bool widescreen = false; // [Video] Widescreen: the Xbox's 16:9 video setting (XGetVideoFlags' widescreen bit).
    int master_volume = 100; // 0..100
    unsigned language = 0; // DAT_0042A710 index, see gbtg/language.h
};

// Beside the executable, then the working directory. Writes a default beside
// the executable when neither file exists.
HostConfig load_host_config(const std::filesystem::path& argv0);

// Absolute path of the running executable; empty when it cannot be resolved.
std::filesystem::path executable_path(const std::filesystem::path& argv0);
// The executable's directory, else the working directory. gbtg.ini and gbtg.log
// live here.
std::filesystem::path host_exe_dir(const std::filesystem::path& argv0);

} // namespace gbtg
