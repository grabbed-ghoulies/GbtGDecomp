#pragma once

#include "gbtg/entity.h"
#include <functional>
#include <vector>

namespace gbtg
{
// One DAT_00511B50 player (actor +0x94C, mode 2) as 0x0013E980 reads it.
struct GhoulySpawnPlayerLink
{
    const Actor* actor = nullptr;
    i32 disabled_ce0 = 0; // Actor +0xCE0.
    i16 mode_cdc = 0; // Actor +0xCDC.
    u32 state_93c = 0; // Actor +0x93C.
    Vec3 previous_position{}; // [actor+0x95C]+0x24.
};
// The native scene's DAT_00511B50 walk, in list order. Unset outside a native scene.
extern std::function<std::vector<GhoulySpawnPlayerLink>()> g_spawn_trigger_players;
// DAT_00512218: the powerup table's Invisibility row (DAT_00512140 kind 9) +0 active, which 0x00123E70 sets.
// The native scene binds it; unset outside one.
extern const u32* g_runtime_invisibility;
u32 runtime_invisibility_active(); // DAT_00512218
i32 check_ghouly_spawn_trigger(const Vec3& query, float radius, float chance,
    u32& random_state); // Xbox 0x0013E980, EBX query, XMM0 radius
}
