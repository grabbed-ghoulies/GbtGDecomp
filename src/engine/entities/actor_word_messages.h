#pragma once
#include "actor_base_messages.h"

namespace gbtg
{
struct RuntimeActorWordBindings
{
    std::function<std::span<const u8>(u32)> memory_at;
    std::function<void*(u32)> reference_at;
    std::function<const AnimationRecord*(u32)> animation_record_at;
};
// Pointer-width adapter from Xbox message bytes into the existing translated
// base message handler. These mappings never create missing actors/resources.
void dispatch_base_actor_word_message(ActorBaseContext& context, std::span<u32> message,
    const RuntimeActorWordBindings& bindings);
}
