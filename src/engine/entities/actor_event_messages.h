#pragma once

#include "actor_scare_requests.h"
#include "actor_trail_requests.h"
#include "../audio/event_voices.h"
#include "../graphics/actor_effect_stream.h"

namespace gbtg
{
struct ActorEventContext
{
    RuntimeActorStartupState& state;
    ActorEffectStreamContext& effects;
    ActorTrailContext& trails;
    ActorScareContext& scares;
    AudioEventVoices& voices;
    std::span<u32> shader_states;
    float& trail_timer; // actor +1DC
    float& scare_intensity; // raw event interpretation of actor +958
    float& scare_scale; // raw event interpretation of actor +95C
};
void dispatch_actor_animation_event(const ActorAnimationMessage& message,
    ActorEventContext& context); // Xbox 0x0003A380
}
