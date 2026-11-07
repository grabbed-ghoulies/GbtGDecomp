#pragma once
#include "../entities/actor_clock.h"
#include "../entities/bounded_channels.h"
#include "effect_colour_channels.h"
#include "gbtg/anim.h"
#include "gbtg/resource_error.h"
#include "gbtg/retail_field.h"
#include <array>
#include <functional>

namespace gbtg
{
// A trail record (0x84 bytes, 50 in DAT_005068D8): a dust puff that waits (phase 1), then plays its texture's frames
// while it grows and fades (phase 2). 0x000EBCB0 queues the live phase-2 records as billboards.
struct EffectTrailRecord
{
    Vec3 position{}; // +0, the billboard's position
    float rotation = 0.f; // +0xC, the billboard's rotation
    EffectColourChannels channels{}; // +0x10
    float scale = 0.f; // +0x4C, the sizes' scale
    float size = 0.f; // +0x50, the billboard's width and height
    float target_size = 0.f; // +0x54
    float size_rate = 0.f; // +0x58
    BoundedChannel frames{}; // +0x5C, over the texture's frames; its value is the billboard's frame
    u8 phase = 0; // +0x70
    u8 type = 0; // +0x71
    std::array<u8, 2> unk_72{}; // +0x72, never read or written
    float phase_time = 0.f; // +0x74, the phase's time left
    float life = 0.f; // +0x78
    i32 next_free = 0; // +0x7C, the next free record; -2 while live, -1 at the end
    u32 unk_80 = 0; // +0x80, never read or written by translated code
};
static_assert(sizeof(EffectTrailRecord) == 0x84);
constexpr size_t kEffectTrailCount = 50;

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
    EffectTrailRecord* particles = nullptr; // DAT_005068D8.
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
    std::span<EffectTrailRecord> particles; // DAT_005068D8.
    std::span<const u8> types; // DAT_005068E0, stride 0xA0.
    i32& free_head; // DAT_005068DC.
    u32& random;
    const i32& pal;
    const i32& override_pal;
    std::function<std::span<const u8>(u32)> resource;
};
void set_effect_trail_phase(EffectTrailRecord& particle, i32 phase,
    RuntimeEffectTrails& context); // Xbox 0x000EB6C0
void tick_effect_trail(EffectTrailRecord& particle, RuntimeEffectTrails& context,
    const RuntimeActorClock& clock); // Xbox 0x000EB9F0
}
