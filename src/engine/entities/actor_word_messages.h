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
// Message 0xB's words {0xB, slot, the 0x000306E0 mix at +8, the animation's name at +0xB8} as the native message;
// its events name and animation name point into `message`.
ActorAnimationMixMessage decode_actor_animation_mix_message(std::span<const u32> message,
    const RuntimeActorWordBindings& bindings);
}
