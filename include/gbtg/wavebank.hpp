#pragma once

#include "gbtg/types.hpp"

namespace gbtg {

struct WaveEntry {
    std::string name;
    u32 flags = 0;
    u32 duration = 0;
    u32 play_offset = 0;
    u32 play_length = 0;
    u32 samples_per_sec = 0;
    u32 channels = 0;
    u32 bits = 0;
    u32 codec = 0;
    std::vector<u8> pcm;  // filled for uncompressed PCM only
};

struct WaveBank {
    std::string path;
    u32 version = 0;
    std::vector<WaveEntry> entries;
};

WaveBank parse_wavebank(const std::filesystem::path& path, int max_pcm_entries = 0);
void write_wav(const std::filesystem::path& path, const WaveEntry& e);

}  // namespace gbtg
