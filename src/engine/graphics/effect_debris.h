#pragma once

#include "effect_colour_channels.h"
#include "gbtg/entity.h"
#include "gbtg/anim.h"
#include "gbtg/resource_error.h"
#include "../entities/actor_clock.h"
#include "gbtg/retail_field.h"
#include <array>
#include <span>

namespace gbtg
{
// A debris record (0x74 bytes, 50 in DAT_005068EC): a splat that grows, holds and fades (phases 1..3).
struct EffectDebrisRecord
{
    Vec3 position{}; // +0
    Vec3 euler{}; // +0xC
    float size = 0.f; // +0x18, drawn from the type's size range
    float growth_rate = 0.f; // +0x1C
    float growth = 0.f; // +0x20, the size's scale, from 1 towards the type's growth
    EffectColourChannels channels{}; // +0x24
    u8 phase = 0; // +0x60
    std::array<u8, 3> unk_61{}; // +0x61, never read or written
    float phase_time = 0.f; // +0x64, the phase's time left
    i32 next_free = 0; // +0x68, the next free record; -2 while live, -1 at the end
    u32 type = 0; // +0x6C
    u32 frame = 0; // +0x70, the frame it was allocated (the oldest is taken when none is free)
};
static_assert(sizeof(EffectDebrisRecord) == 0x74);
constexpr size_t kEffectDebrisCount = 50;

// A debris type (0xAC bytes, ten in aid_misc_ghoulies_fx_cheap_splat). The ranges are low and high.
namespace effect_debris_type_field
{
constexpr RetailField<std::array<char, 0x80>, 0x00> texture{}; // an AID, or empty for none
constexpr RetailField<u32, 0x80> texture_address{}; // written when the textures are bound
constexpr RetailField<std::array<u8, 4>, 0x84> colour{}; // red, green, blue, alpha
constexpr RetailField<std::array<float, 2>, 0x88> size{};
constexpr RetailField<float, 0x90> growth{};
constexpr RetailField<std::array<float, 2>, 0x94> grow_time{};
constexpr RetailField<std::array<float, 2>, 0x9C> hold_time{};
constexpr RetailField<std::array<float, 2>, 0xA4> fade_time{};
constexpr u32 kSize = 0xAC;
static_assert(retail_fits(kSize, fade_time));
} // namespace effect_debris_type_field
constexpr RetailRecord<0, effect_debris_type_field::kSize, 10> effect_debris_types{};

struct RuntimeEffectDebrisStorage
{
    EffectDebrisRecord* particles = nullptr; // DAT_005068EC.
    std::span<const u8> types; // DAT_005068E8.
    std::array<std::span<const u8>, 10> textures; // Native views of type +0x80.
    i32 free_head = 0; // DAT_005068F0.
};
void initialize_effect_debris(RuntimeEffectDebrisStorage& storage,
    AnimationResourceRegistry& registry, ResourceErrorState& error,
    const std::function<u32(const u8*)>& address); // Xbox 0x000EC2D0
void release_effect_debris(RuntimeEffectDebrisStorage& storage,
    AnimationResourceRegistry& registry); // Xbox 0x000EC3A0
struct RuntimeEffectDebris
{
    std::span<EffectDebrisRecord> particles; // DAT_005068EC.
    std::span<const u8> types; // DAT_005068E8, stride 0xAC.
    i32& free_head; // DAT_005068F0.
    const u32& frame; // DAT_004DEA58.
    u32& random; // DAT_0054DD7C.
};
i32 oldest_effect_debris(const RuntimeEffectDebris& context); // Xbox 0x000EBD80
EffectDebrisRecord* allocate_effect_debris(RuntimeEffectDebris& context); // Xbox 0x000EBDB0
void set_effect_debris_phase(EffectDebrisRecord& particle, i32 phase,
    RuntimeEffectDebris& context); // Xbox 0x000EBE50
void tick_effect_debris(EffectDebrisRecord& particle, RuntimeEffectDebris& context,
    const RuntimeActorClock& clock); // Xbox 0x000EC0B0
void spawn_effect_debris(i32 type, const Vec3& position, const Vec3& euler,
    RuntimeEffectDebris& context); // Xbox 0x000EC580
void sample_effect_collision_pose(const Vec3& source, const Vec3& normal,
    Vec3& position, Vec3& euler); // Xbox 0x000F1040
}
