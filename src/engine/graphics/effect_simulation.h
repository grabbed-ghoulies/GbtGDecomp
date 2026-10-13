#pragma once

#include "effect_collision_commands.h"

namespace gbtg
{
struct RuntimeEffectSimulation
{
    RuntimeEffectCollisionContext& movement;
    RuntimeEffectCollisionCommands& commands;
    const std::function<std::span<const u8>(u32)>& resolve;
    const i32& pal;
    const i32& override_pal;
};
u32 simulate_actor_effect_particle(std::span<u8> particle, ActorEffectNode& owner,
    float step, u32& collided, float& previous_collisions,
    RuntimeEffectSimulation& context); // Xbox 0x000F18E0
}
