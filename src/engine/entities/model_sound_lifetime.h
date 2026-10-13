#pragma once

#include "gbtg/model_runtime.h"
#include "gbtg/sound_runtime.h"

namespace gbtg
{
struct RuntimeModelSoundObject
{
    u8* allocation; // Retail object base.
    std::span<const u8> resource_134;
    std::span<const u8> resource_144;
    RuntimeModelInstance* model_158;
    RuntimeActorSound sound_2dc;
};
struct RuntimeModelSoundRelease
{
    RuntimeActorSoundQueues& sounds;
    AnimationResourceRegistry& registry;
    RuntimeModelDeferredPool& deferred;
    u32& fence;
    RuntimeShaderRegistrationState& shaders;
    const std::function<void(u8*)>& heap_free;
};
void release_runtime_model_sound_object(RuntimeModelSoundObject& object,
    RuntimeModelSoundRelease& services); // Xbox 0x000ED2E0
}
