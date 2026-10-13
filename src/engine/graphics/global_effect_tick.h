#pragma once
#include "effect_tick.h"
#include "effect_debris.h"
#include "effect_trails.h"
#include "../entities/model_sound_lifetime.h"

namespace gbtg
{
struct RuntimeGlobalEffectTick
{
    std::array<ActorEffectNode*, 150>& nodes; // DAT_00506930.
    RuntimeEffectTickContext& effects;
    RuntimeModelSoundRelease& release;
    RuntimeEffectDebris& debris;
    RuntimeEffectTrails& trails;
    const RuntimeActorClock& clock;
};
// The frame caller ignores the incidental final EAX value.
void tick_global_actor_effects(RuntimeGlobalEffectTick& context); // Xbox 0x000ED480
void detach_actor_from_global_effects(void* actor,
    std::array<ActorEffectNode*, 150>& nodes); // Xbox 0x000ED5B0
}
