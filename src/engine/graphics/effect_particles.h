#pragma once
#include "actor_effect_nodes.h"

namespace gbtg
{
void initialize_actor_effect_particle(std::span<u8> particle, const ActorEffectNode& effect,
    u32& random); // Xbox 0x000EF8C0
void initialize_actor_effect_particle_frames(std::span<u8> particle, const ActorEffectNode& effect,
    u32& random, i32 pal, i32 pal_override); // Xbox 0x000EFB20
u8* allocate_actor_effect_particle(ActorEffectNode& effect, u32 owner_address, u32 user,
    u32& random, i32 pal, i32 pal_override); // Xbox 0x000EFE30
}
