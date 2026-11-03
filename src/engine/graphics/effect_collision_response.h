#pragma once
#include "gbtg/entity.h"
#include <span>
#include "../camera/plane_collision.h"
#include "../entities/actor_clock.h"

namespace gbtg
{
struct EffectParticle; // effect_particles.h
struct RuntimeEffectCollision
{
    u32 kind = 0; // DAT_00506918.
    u8 surface = 0; // DAT_0050691C.
    u32 flags = 0; // DAT_00506920.
    Vec3 normal; // DAT_00506924.
};
struct RuntimeEffectCollisionContext
{
    RuntimeEffectCollision& collision;
    const RuntimeActorClock& clock;
    const entity_detail::DisplayView& view; // DAT_005077A8.
    const Vec3& plane; // DAT_00433FFC.
    const u8& bounded; // DAT_0050EA00.
    const Vec3& minimum; // DAT_0050EA04.
    const Vec3& maximum; // DAT_0050EA10.
    u32& random;
};
bool move_actor_effect_particle(EffectParticle& particle, RuntimeEffectCollisionContext& context,
    const std::function<void()>& hit_callback); // Xbox 0x000ECE50
i32 probe_actor_effect_world(Vec3& source, Vec3& candidate, float radius, i32 kind,
    u32 mask, u32 grid_only, RuntimeEffectCollision& collision); // Xbox 0x000ECA20
bool probe_actor_effect_particle(EffectParticle& particle, Vec3& candidate,
    RuntimeEffectCollision& collision); // Xbox 0x000ECBD0
void reflect_actor_effect_particle(EffectParticle& particle, u32 surface_flags,
    const Vec3& normal, u32& random); // Xbox 0x000ECC30
}
