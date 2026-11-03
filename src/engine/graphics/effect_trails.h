#pragma once
#include "../entities/actor_clock.h"
#include "gbtg/anim.h"
#include "gbtg/resource_error.h"
#include "gbtg/retail_field.h"
#include <array>
#include <functional>

namespace gbtg
{
// A trail record (0x84 bytes, 50 in DAT_005068D8): a dust puff that waits (phase 1), then plays its texture's frames
// while it grows and fades (phase 2).
namespace effect_trail_field
{
constexpr RetailRecord<0x10, 0x3C> channels{}; // effect_channel_field
constexpr RetailField<float, 0x4C> scale{}; // the sizes' scale
constexpr RetailField<float, 0x50> size{};
constexpr RetailField<float, 0x54> target_size{};
constexpr RetailField<float, 0x58> size_rate{};
constexpr RetailRecord<0x5C, 0x14> frames{}; // bounded_channel_field over the texture's frames
constexpr RetailField<u8, 0x70> phase{};
constexpr RetailField<u8, 0x71> type{};
constexpr RetailField<float, 0x74> phase_time{}; // the phase's time left
constexpr RetailField<float, 0x78> life{};
constexpr RetailField<i32, 0x7C> next_free{}; // the next free record; -2 while live, -1 at the end
constexpr u32 kSize = 0x84;
} // namespace effect_trail_field
constexpr RetailRecord<0, effect_trail_field::kSize, 50> effect_trail_pool{};

// A trail type (0xA0 bytes, four in aid_misc_ghoulies_fx_cheap_dust). Each puff's colour is drawn between the two.
namespace effect_trail_type_field
{
constexpr RetailField<u32, 0x00> texture_address{}; // written when the textures are bound
constexpr RetailField<std::array<char, 0x80>, 0x04> texture{}; // an AID, or empty for none
constexpr RetailField<std::array<u8, 4>, 0x84> first_colour{}; // red, green, blue, alpha
constexpr RetailField<std::array<u8, 4>, 0x88> second_colour{};
constexpr RetailField<float, 0x94> start_size{}; // of the record's scale
constexpr RetailField<float, 0x98> end_size{};
constexpr RetailField<float, 0x9C> frame_rate{};
constexpr u32 kSize = 0xA0;
static_assert(retail_fits(kSize, frame_rate));
} // namespace effect_trail_type_field
constexpr RetailRecord<0, effect_trail_type_field::kSize, 4> effect_trail_types{};

struct RuntimeEffectTrailStorage
{
    u8* particles = nullptr; // DAT_005068D8.
    std::span<const u8> types; // DAT_005068E0.
    std::array<std::span<const u8>, 4> textures; // Native type +0 resource pointers.
    i32 free_head = 0; // DAT_005068DC.
};
void bind_effect_trail_textures(RuntimeEffectTrailStorage& storage,
    AnimationResourceRegistry& registry, ResourceErrorState& error,
    const std::function<u32(const u8*)>& address); // Xbox 0x000EBB10
void initialize_effect_trails(RuntimeEffectTrailStorage& storage,
    AnimationResourceRegistry& registry, ResourceErrorState& error,
    const std::function<u32(const u8*)>& address); // Xbox 0x000EBB80
void release_effect_trails(RuntimeEffectTrailStorage& storage,
    AnimationResourceRegistry& registry); // Xbox 0x000EBBF0
struct RuntimeEffectTrails
{
    std::span<u8> particles; // DAT_005068D8, 50 records of 0x84 bytes.
    std::span<const u8> types; // DAT_005068E0, stride 0xA0.
    i32& free_head; // DAT_005068DC.
    u32& random;
    const i32& pal;
    const i32& override_pal;
    std::function<std::span<const u8>(u32)> resource;
};
void set_effect_trail_phase(std::span<u8> particle, i32 phase,
    RuntimeEffectTrails& context); // Xbox 0x000EB6C0
void tick_effect_trail(std::span<u8> particle, RuntimeEffectTrails& context,
    const RuntimeActorClock& clock); // Xbox 0x000EB9F0
}
