#pragma once

#include "event_voices.h"

namespace gbtg
{
struct RuntimeActorSoundTick
{
    AudioEventVoices& voices;
    const Vec3& listener; // DAT_005077A8 +0.
    const std::function<void(Actor&, u32, RuntimeActorSound&)>& message;
};
void update_active_actor_sound_voices(RuntimeActorSoundGroup& group,
    RuntimeActorSoundTick& context); // Xbox 0x0012A510
void sort_pending_actor_sounds(RuntimeActorSoundGroup& group,
    const Vec3& listener); // Xbox 0x0012A6E0
void tick_runtime_actor_sounds(RuntimeActorSoundQueues& queues,
    RuntimeActorSoundTick& context); // Xbox 0x0012A150
}
