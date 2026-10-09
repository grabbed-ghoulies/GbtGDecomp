#pragma once

#include "gbtg/entity.h"

namespace gbtg
{
struct GhoulySpawnPlayerLink
{
    GhoulySpawnPlayerLink* previous = nullptr;
    GhoulySpawnPlayerLink* next = nullptr;
    Actor* actor = nullptr;
    i32 disabled_ce0 = 0;
    i16 mode_cdc = 0;
    void* state_93c = nullptr;
    const Vec3* previous_position = nullptr; // [actor+0x95C]+0x24
};
i32 check_ghouly_spawn_trigger(const Vec3& query, float radius, float chance,
    i32 blocked_players, u32& random_state); // Xbox 0x0013E980
}
