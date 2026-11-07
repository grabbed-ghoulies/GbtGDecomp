#pragma once
#include "actor_effect_nodes.h"

namespace gbtg
{
struct EffectParticle; // effect_particles.h
bool actor_effect_command_condition(std::span<const u8> command,
    const EffectParticle& particle); // Xbox 0x000F0300
u32 find_actor_effect_fallback_command(const EffectParticle& particle, ActorEffectNode& effect,
    const std::function<std::span<const u8>(u32)>& resolve); // Xbox 0x000F0FF0
}
