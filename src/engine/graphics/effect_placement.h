#pragma once
#include "gbtg/entity.h"
#include <span>

namespace gbtg
{
struct EffectParticle; // effect_particles.h
void jitter_actor_effect_position(Vec3& position, std::span<const u8> effect,
    u32& random); // Xbox 0x000EEA70
void initialize_actor_effect_velocity(EffectParticle& particle, std::span<const u8> effect,
    u32& random); // Xbox 0x000EE5C0
// Scratch retains the retail local's incoming bytes for an unsupported volume kind.
void place_actor_effect_particle(std::span<const u8> effect, EffectParticle& particle,
    std::span<const u8> owner, Vec3& scratch, u32& random); // Xbox 0x000EED50
}
