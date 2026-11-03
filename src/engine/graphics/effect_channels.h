#pragma once
#include "../entities/actor_clock.h"
#include "gbtg/retail_field.h"

#include <array>

namespace gbtg
{
// The colour channels (0x3C bytes: an effect particle's +0x60, a debris record's +0x24, a trail record's +0x10):
// red, green, blue, alpha and a fifth, each approaching its target at its rate and kept in 0..255 (0x000F1F60).
// A zero rate leaves the channel alone.
namespace effect_channel_field
{
constexpr RetailField<std::array<float, 5>, 0x00> values{};
constexpr RetailField<float, 0x00> red{};
constexpr RetailField<float, 0x04> green{};
constexpr RetailField<float, 0x08> blue{};
constexpr RetailField<float, 0x0C> alpha{};
constexpr RetailField<float, 0x10> unk_10{};
constexpr RetailField<std::array<float, 5>, 0x14> targets{};
constexpr RetailField<float, 0x14> target_red{};
constexpr RetailField<float, 0x18> target_green{};
constexpr RetailField<float, 0x1C> target_blue{};
constexpr RetailField<float, 0x20> target_alpha{};
constexpr RetailField<float, 0x24> target_unk_10{};
constexpr RetailField<std::array<float, 5>, 0x28> rates{};
constexpr RetailField<float, 0x34> alpha_rate{};
constexpr RetailField<float, 0x38> unk_10_rate{};
constexpr u32 kSize = 0x3C;
static_assert(retail_fits(kSize, rates));
} // namespace effect_channel_field

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
