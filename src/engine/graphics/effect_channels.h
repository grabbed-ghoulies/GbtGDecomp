#pragma once
#include "../entities/actor_clock.h"

namespace gbtg
{
void configure_actor_effect_bounded_channels(std::span<u8> particle, const Vec3& first,
    const Vec3& second, const Vec3& position, const Vec3& rates, i32 random_position,
    const i32& pal, const i32& override_pal, u32& random); // Xbox 0x000F1D20
bool actor_effect_channels_are_active(std::span<const u8> channels); // Xbox 0x000F2430
void target_actor_effect_color(std::span<u8> channels, u8 red, u8 green, u8 blue,
    float duration); // Xbox 0x000F22F0
void target_actor_effect_alpha(std::span<u8> channels, u8 alpha,
    float duration); // Xbox 0x000F23E0
void configure_actor_effect_oscillation(std::span<u8> particle, const Vec3& rate,
    const Vec3& phase, const Vec3& lower, const Vec3& upper, i32 random_phase,
    u32& random); // Xbox 0x000F1AF0
void tick_actor_effect_oscillation(std::span<u8> channels, float step); // Xbox 0x00106A80
void approach_actor_effect_channels(std::span<u8> channels,
    const RuntimeActorClock& clock); // Xbox 0x000F1F60
}
