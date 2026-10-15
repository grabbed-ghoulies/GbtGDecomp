#include "runtime_profile.h"
#include "../scripts/script_conditions.h"

#include <algorithm>

namespace gbtg
{
namespace
{
// Boot 0x00033030: DAT_00506C70 is 0xD bytes (kind 4, size 1) and DAT_00506C6C 0x14 bytes (kind 0x3E, size 8).
std::array<u8, 8> g_progress_bytes{};
std::array<u8, 1> g_unlock_bytes{};
}

RuntimeProfile g_runtime_profile{{0x3E, 8, g_progress_bytes}, {4, 1, g_unlock_bytes}, g_scene_flags, {}, {}};

void clear_runtime_profile_block(std::span<u8> bytes)
{
    std::fill(bytes.begin(), bytes.end(), u8(0));
}
}
