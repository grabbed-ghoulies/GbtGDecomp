#pragma once
#include "../entities/actor_clock.h"
#include "gbtg/anim.h"
#include "gbtg/resource_error.h"
#include <functional>

namespace gbtg
{
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
