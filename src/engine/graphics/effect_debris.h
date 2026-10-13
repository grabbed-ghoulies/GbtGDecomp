#pragma once

#include "gbtg/entity.h"
#include "gbtg/anim.h"
#include "gbtg/resource_error.h"
#include "../entities/actor_clock.h"
#include <span>

namespace gbtg
{
struct RuntimeEffectDebrisStorage
{
    u8* particles = nullptr; // DAT_005068EC.
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
    std::span<u8> particles; // DAT_005068EC, 50 records of 0x74 bytes.
    std::span<const u8> types; // DAT_005068E8, stride 0xAC.
    i32& free_head; // DAT_005068F0.
    const u32& frame; // DAT_004DEA58.
    u32& random; // DAT_0054DD7C.
};
i32 oldest_effect_debris(const RuntimeEffectDebris& context); // Xbox 0x000EBD80
std::span<u8> allocate_effect_debris(RuntimeEffectDebris& context); // Xbox 0x000EBDB0
void set_effect_debris_phase(std::span<u8> particle, i32 phase,
    RuntimeEffectDebris& context); // Xbox 0x000EBE50
void tick_effect_debris(std::span<u8> particle, RuntimeEffectDebris& context,
    const RuntimeActorClock& clock); // Xbox 0x000EC0B0
void spawn_effect_debris(i32 type, const Vec3& position, const Vec3& euler,
    RuntimeEffectDebris& context); // Xbox 0x000EC580
void sample_effect_collision_pose(const Vec3& source, const Vec3& normal,
    Vec3& position, Vec3& euler); // Xbox 0x000F1040
}
