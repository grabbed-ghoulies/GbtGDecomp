#pragma once

#include "gbtg/types.h"
#include <array>
#include <functional>
#include <span>

namespace gbtg
{
// DAT_0052C274 and DAT_00512B78: per kind, a 61 x 61 table of event counts by (second tag, first tag), with row 0
// and column 0 holding the totals. The script filter 0x00116950 records kinds 0 to 4.
struct RuntimeScriptTallies
{
    static constexpr size_t kind_words = 0xE89; // 0x3A24 bytes per kind.
    std::array<u32, 5 * kind_words> first{}; // DAT_0052C274.
    std::array<u32, 5 * kind_words> second{}; // DAT_00512B78.
};
extern RuntimeScriptTallies g_script_tallies;

// The kill counters 0x0012ACF0 raises for kind 0.
struct RuntimeScriptKillCounters
{
    i32* first_tag_kills; // DAT_00506C80.
    i32* first_tag_total; // DAT_00506C8C.
    i32* second_tag_kills; // DAT_00545970.
};
// Bound by the scene's actor services.
extern std::function<RuntimeScriptKillCounters()> g_script_kill_counters;
// A scene actor's instance bytes by retail address (the filter reads each actor's +6 tag), bound by the scene's
// actor services.
extern std::function<std::span<const u8>(u32 address)> g_script_actor_bytes;
// Event 0xB0's door: 0x00129ED0 finds the actor with the door's id; when it and the 0x001292D0 player exist, the
// player's world position (0x000412A0, pose +4) and euler (0x00041820, pose +0x10) go in, and the pose goes to the
// actor's vtable +0xC. Returns the scene's marker records (DAT_0051108C). Bound by the scene's actor services.
extern std::function<const u8*(u16 door, std::span<u32, 0x42> pose)> g_script_door_pose;

void tally_runtime_script_pair(u32 kind, std::span<u32> table, u16 second,
    u16 first); // Xbox 0x0012AF80, EAX kind, EDX table, DI first
void tally_runtime_script_event(u16 first, u16 second, u32 kind); // Xbox 0x0012ACF0, AX first, BX second, ESI kind
void tally_runtime_script_hit(u32 source, u32 target, u32 unblocked,
    u32 third); // Xbox 0x00116C10, EDI source, EAX target, EDX unblocked
}
