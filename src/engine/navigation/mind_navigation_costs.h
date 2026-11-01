#pragma once
#include "spawn_candidates.h"
#include "gbtg/entity.h"

namespace gbtg
{
GhoulySpawnPool* collect_runtime_navigation_costs(i32 floor, const Vec3& center,
    float radius); // Xbox 0x00110110
GhoulySpawnPool* suspend_runtime_mind_target_costs(std::span<const u8> actor,
    std::span<const u8> body, const Actor& body_actor,
    std::span<const u8> target); // Xbox 0x0007B490
void restore_runtime_mind_target_costs(std::span<const u8> actor,
    GhoulySpawnPool* costs); // Xbox 0x0007B510
}
