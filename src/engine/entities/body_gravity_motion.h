#pragma once
#include "body_damage_tables.h"
#include "actor_clock.h"
#include "gbtg/anim.h"

namespace gbtg
{
float runtime_body_motion_speed(std::span<const u8> body, std::span<const u8> actor,
    const RuntimeBodyDamageTables& tables); // Xbox 0x000259C0
void limit_runtime_body_backward_motion(Vec3& motion, std::span<const u8> body,
    const EntityBitBlock& flags, float lower); // Xbox 0x0002B300
void step_runtime_gravity_motion(std::span<u8> body, std::span<const u8> actor_bytes,
    Actor& actor, const ActorAnimationState& animation, const RuntimeActorClock& clock,
    const EntityBitBlock& flags, const RuntimeBodyDamageTables& tables,
    std::span<float, 8> phases, float ratio_scale, float backward_lower,
    float turn_threshold, u32 limit, u32 reverse); // Xbox 0x0002AE90
}
