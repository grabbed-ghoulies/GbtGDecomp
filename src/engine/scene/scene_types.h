#pragma once

#include "gbtg/game.h"
#include "gbtg/entity.h"

#include <array>
#include <bit>
#include <cstddef>
#include <vector>

namespace gbtg
{
namespace scene_detail
{
}
struct ScriptCond;
struct LiveScript;
struct SceneFlagBlock;
struct TransitionSlot31478;
using namespace scene_detail;

// Condition opcodes are the index into the 27-entry table at 0x00314258.
// The paired size is what 0x00124C80 copies into the stride-0x20 record.
struct ScriptCond
{
    u8 raw[0x20]{};
};

inline constexpr u32 kScriptCondSize[27] = {
    0x10, 0x10, 0x08, 0x10, 0x18, 0x14, 0x10, 0x0C, 0x0C, 0x0C, 0x14, 0x14, 0x10, 0x10,
    0x10, 0x14, 0x10, 0x1C, 0x10, 0x20, 0x08, 0x08, 0x08, 0x10, 0x10, 0x10, 0x0C,
};

// DAT_0050E4DC. 0x00116810 allocates the 0x214-byte node. +4 is next. +8 is
// the type 0x00128A00 compares with 0x7D before it calls the handlers.
// DAT_0050E4D8 is the anchor immediately before that head. 0x00116810 stores
// the anchor as the first node's prev, so 0x000E9210's store at prev+4 writes
// the new head.
struct LiveScript
{
    LiveScript* prev = nullptr;
    LiveScript* next = nullptr;
    u32 type = 0;
    // Xbox +0xC. 0x00132720 compares this word with the word at record+8.
    u16 word_c = 0;
    // Xbox +0x10. 0x00132D00 reads this dword: mode 1 copies it, mode 2
    // stores whether it is zero. The type dword is at +8, so this is the
    // dword at that address +8.
    u32 dword_10 = 0;
    // Xbox +0x14. 0x001168F0 compares this word with DX. The type dword is
    // at +8, so the compare is the word at that address +0xC.
    u16 word_14 = 0;
    std::array<u8, 0x200> message{};
    float timestamp = 0.f; // Xbox +0x210
};

// DAT_00506c74. +8 is the flag-byte pointer. 0x00132ED0 tests bit 0x8 of the
// first byte. The payload is the two bytes 0x000F2B60 publishes.
struct SceneFlagBlock
{
    i32 kind = 0; // +0
    i32 size = 0; // +4
    u8* bytes = nullptr; // +8
};

// 'aid_' as a little-endian dword. 0x0011EB30 clears the name when it matches.
inline constexpr u32 kAidPrefixEd8 = 0x5f646961u;

// Image at DAT_00431478. Stride 0x110. +0 indexes the 0x18-byte records at
// DAT_003129AC. +4 is the aid string 0x0012C5D0 compares with 'aid_'.
// +0x104 is the dword copied into DAT_00546330. +0x10C is the gate
// 0x0012C340 tests. The eighteenth record is the sentinel whose index is -1.
struct TransitionSlot31478
{
    i32 handler = 0; // +0
    char aid[0x80]{}; // +4
    char closing_aid[0x80]{}; // +0x84
    u32 at_104 = 0; // +0x104
    i32 at_108 = 0; // +0x108
    i32 at_10c = 0; // +0x10C
};

inline constexpr TransitionSlot31478 kTransition31478[] = {
    {0, "", "", 0x00000000u, 0, 1},
    {1, "", "", 0x00000000u, 0, 0},
    {1, "", "", 0x00000000u, 1, 0},
    {1, "aid_texture_ghoulies_transition_chaptertitles_chapter1", "", 0x00000000u, 2, 0},
    {1, "aid_texture_ghoulies_transition_chaptertitles_chapter2", "", 0x00000000u, 2, 0},
    {1, "aid_texture_ghoulies_transition_chaptertitles_chapter3", "", 0x00000000u, 2, 0},
    {1, "aid_texture_ghoulies_transition_chaptertitles_chapter4", "", 0x00000000u, 2, 0},
    {1, "aid_texture_ghoulies_transition_chaptertitles_chapter5", "", 0x00000000u, 2, 0},
    {1, "", "", 0x00000000u, 3, 0},
    {1, "", "", 0x00000000u, 4, 0},
    {1, "", "", 0x00000000u, 5, 0},
    {1, "", "", 0x00000000u, 6, 0},
    {1, "", "", 0x00000000u, 7, 0},
    {1, "aid_texture_ghoulies_unlockableart_whereitbegan", "", 0x00000000u, 13, 0},
    {1, "aid_texture_ghoulies_unlockableart_whereitbegan", "", 0x00000000u, 14, 0},
    {2, "aid_texture_ghoulies_transition_blood_down", "aid_texture_ghoulies_transition_blood_down", 0x3fc00000u, -1, 1},
    {3, "", "", 0x3e4ccccdu, 0, 1},
    {-1, "", "", 0x00000000u, 0, 0},
};

} // namespace gbtg
