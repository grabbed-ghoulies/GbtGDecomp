#pragma once
#include "../entities/actor_clock.h"
#include "effect_colour_channels.h"
#include "effect_particles.h"
#include "gbtg/retail_field.h"

#include <array>

namespace gbtg
{
void configure_actor_effect_bounded_channels(EffectParticle& particle, const Vec3& first,
    const Vec3& second, const Vec3& position, const Vec3& rates, i32 random_position,
    const i32& pal, const i32& override_pal, u32& random); // Xbox 0x000F1D20
bool actor_effect_channels_are_active(const EffectColourChannels& channels); // Xbox 0x000F2430
void target_actor_effect_color(EffectColourChannels& channels, u8 red, u8 green, u8 blue,
    float duration); // Xbox 0x000F22F0
void target_actor_effect_alpha(EffectColourChannels& channels, u8 alpha,
    float duration); // Xbox 0x000F23E0
void configure_actor_effect_oscillation(EffectParticle& particle, const Vec3& rate,
    const Vec3& phase, const Vec3& lower, const Vec3& upper, i32 random_phase,
    u32& random); // Xbox 0x000F1AF0
void tick_actor_effect_oscillation(EffectOscillation& oscillation, float step); // Xbox 0x00106A80
void approach_actor_effect_channels(EffectColourChannels& channels,
    const RuntimeActorClock& clock); // Xbox 0x000F1F60
}
