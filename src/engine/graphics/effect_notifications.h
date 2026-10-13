#pragma once

#include "actor_effect_stream.h"
#include "../audio/event_voices.h"

namespace gbtg
{
void emit_positioned_actor_effect(const char* name, const Vec3& position,
    ActorEffectStreamContext& context); // Xbox 0x0013E610
void notify_actor_effect_particle(const ActorEffectNode& owner, u32 collided,
    const Vec3& position, const u32& frame, u32& last_sound_frame,
    AudioEventVoices& voices, ActorEffectStreamContext& context); // Xbox 0x000EE560
}
