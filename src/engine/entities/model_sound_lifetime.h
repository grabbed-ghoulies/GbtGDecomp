#pragma once

#include "gbtg/model_runtime.h"
#include "gbtg/sound_runtime.h"
#include "../graphics/actor_effect_nodes.h"

namespace gbtg
{
struct RuntimeModelSoundRelease
{
    RuntimeActorSoundQueues& sounds;
    AnimationResourceRegistry& registry;
    RuntimeModelDeferredPool& deferred;
    u32& fence;
    RuntimeShaderRegistrationState& shaders;
    const std::function<void(u8*)>& heap_free;
};
// Releases the emitter's sound, texture (+0x144), model (+0x158) and script (+0x134), then frees the node.
void release_actor_effect_node(ActorEffectNode& effect,
    RuntimeModelSoundRelease& services); // Xbox 0x000ED2E0
}
