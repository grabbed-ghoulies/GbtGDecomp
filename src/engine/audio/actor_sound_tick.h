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
// The scene start's named sounds (kind 7 markers, DAT_0051254C): each row's sound plays at its +0x80 position and
// keeps its voice at +0x8C, -1 when none was free.
void start_runtime_scene_named_sounds(RuntimeActorSoundQueues& queues, AudioEventVoices& voices); // Xbox 0x0012A1E0
}
