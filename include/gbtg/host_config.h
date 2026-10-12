#pragma once

#include <filesystem>

namespace gbtg
{

// PC host settings from gbtg.ini. Reconstructed game code does not read this.
struct HostConfig
{
    int width = 960;
    int height = 540;
    int vsync = 1;
    int master_volume = 100; // 0..100
    unsigned language = 0; // DAT_0042A710 index, see gbtg/language.h
};

// Beside the executable, then the working directory. Writes a default beside
// the executable when neither file exists.
HostConfig load_host_config(const std::filesystem::path& argv0);

} // namespace gbtg
