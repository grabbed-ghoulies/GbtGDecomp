#pragma once

#include "effect_simulation.h"

namespace gbtg
{
struct RuntimeEffectPassScratch
{
    Vec3 placement; // Incoming EED50 local for unsupported volume kinds.
    u32 collided; // EF0B0 stack +0xC; F18E0 leaves it untouched on expiry.
    float previous_collisions; // EF0B0 stack +0x10; untouched on a hit.
};
struct RuntimeEffectTickContext
{
    RuntimeEffectSimulation& simulation;
    const u32& frame;
    u32& last_sound_frame;
    const std::function<u32(const u8*)>& address;
    const Vec3& camera_direction; // DAT_005077A8 +0x1C.
    RuntimeEffectPassScratch& scratch;
};
void spawn_and_simulate_actor_effect_particle(ActorEffectNode& effect, float step,
    RuntimeEffectTickContext& context); // Xbox 0x000EF260
void tick_active_actor_effect_particles(ActorEffectNode& effect, float step,
    RuntimeEffectTickContext& context); // Xbox 0x000EF0B0
bool tick_actor_effect(ActorEffectNode& effect, float step,
    RuntimeEffectTickContext& context); // Xbox 0x000EF2D0
}
