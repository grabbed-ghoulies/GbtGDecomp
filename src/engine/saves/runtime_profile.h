#pragma once
#include "gbtg/types.h"
#include <array>
#include <span>

namespace gbtg
{
struct SceneFlagBlock;
// A boot-allocated progress block: kind, size and a pointer to its inline bytes (0x000F72D0 blocks).
struct RuntimeProfileBlock
{
    i32 kind = 0; // +0
    i32 size = 0; // +4
    std::span<u8> bytes; // +8
};
// The profile state boot 0x00033030 allocates and game select resets and saves.
struct RuntimeProfile
{
    RuntimeProfileBlock progress; // DAT_00506C6C: kind 0x3E, 8 bytes.
    RuntimeProfileBlock unlocks; // DAT_00506C70: kind 4, 1 byte.
    SceneFlagBlock* scene_flags = nullptr; // DAT_00506C74: kind 9, 2 bytes (0x000F2B60).
    std::array<i32, 6> times{}; // DAT_00506C78..DAT_00506C8C.
    std::array<char16_t, 0x80> name{}; // DAT_00506C90, the current game's save name.
};
extern RuntimeProfile g_runtime_profile;
// Clears a block's bytes (the rep stos over +8 for +4 bytes).
void clear_runtime_profile_block(std::span<u8> bytes);
}
