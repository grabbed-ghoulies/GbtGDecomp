#pragma once
#include "effect_tick.h"
#include "effect_debris.h"
#include "effect_trails.h"
#include "../entities/model_sound_lifetime.h"

namespace gbtg
{
struct RuntimeGlobalEffectTick
{
    ActorEffectSlots& nodes; // DAT_00506930.
    RuntimeEffectTickContext& effects;
    RuntimeModelSoundRelease& release;
    RuntimeEffectDebris& debris;
    RuntimeEffectTrails& trails;
    const RuntimeActorClock& clock;
};
// The frame caller ignores the incidental final EAX value.
void tick_global_actor_effects(RuntimeGlobalEffectTick& context); // Xbox 0x000ED480
// Scene teardown: every effect node is released and its slot cleared, then the debris and trail pools.
void release_global_actor_effects(ActorEffectSlots& nodes, RuntimeModelSoundRelease& release,
    RuntimeEffectDebrisStorage& debris, RuntimeEffectTrailStorage& trails,
    AnimationResourceRegistry& registry); // Xbox 0x000ED450
void detach_actor_from_global_effects(void* actor,
    ActorEffectSlots& nodes); // Xbox 0x000ED5B0
}
