#pragma once

#include "body_action_selection.h"
#include "body_animation_setup.h"
#include "actor_clock.h"
#include "../frontend/dialogue_input.h"
#include "gbtg/actor_runtime.h"
#include "gbtg/sound_runtime.h"

#include <functional>

namespace gbtg
{
// Factory type 0x5E (getter 0x00088520, vtable 0x005056A0): a body state that plays an optional animation and
// sound, can turn the body toward the mind's target, and takes its next key on a timer, at the animation's end, or
// once the animation passes a point and the stick is pushed. +0x14/+0x18 are the empty 0x001628B0 and +0x1C is
// 0x0002B620. State +8 is the body.
//
// State words: +0xC the next key, +0x10 the state clock, +0x18 the fixed next key (parameters +0xC; 0 takes body
// vtable +0x20), +0x1C the body bit it raises (parameters +0x10), +0x20 the timer (parameters +0x20) and +0x24 its
// flag, +0x28 the sound started (parameters +0x24 names it), +0x2C face the mind's target (parameters +0x14) with
// +0x30 added to the angle (parameters +0x18), +0x34 the animation point after which the stick ends the state
// (parameters +0x1C), +0x38 copy actor +0x3C into body +0x318 (parameters +0xAC), +0x3C settle the body's turn
// (parameters +0xA4). Parameters +0xA8 name the animation.
struct RuntimeAnimStateContext
{
    std::span<u8> body;
    Actor& actor; // Body +8.
    RuntimeBodyBitStorage& bits; // Body +0x15C.
    ActorAnimationState& animation; // Actor +0x50.
    const RuntimeActorClock& clock;
    RuntimeBodyActionSelection& actions; // 0x0001AF10 / 0x00027650 views.
    RuntimeActorSoundQueues& sounds;
    float stick_threshold; // DAT_0032B22C.
    std::function<const char*(u32 address)> string_at; // An XBE string the parameters name.
    std::function<u32()> mind_target; // Actor +0x958 +0xC, or 0 when actor +0x958 is 0.
    std::function<const Actor*(u32 address)> actor_at;
    std::function<u16(u32 slot)> body_slot_key; // Body vtable +0x20.
    const RuntimeDialogueInputDevice* input = nullptr; // The tick's second argument (actor +0x960 or zero).
    const RuntimeDialogueInputState* input_state = nullptr;
};

void anim_state_object_sizes(ObjectSizes& output); // Xbox 0x00088500
bool initialize_runtime_anim_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimeAnimStateContext& context); // Xbox 0x00088580
void destroy_runtime_anim_state(std::span<u8> state, RuntimeAnimStateContext& context); // Xbox 0x00088660
void message_runtime_anim_state(std::span<u8> state, std::span<const u8> message,
    RuntimeAnimStateContext& context); // Xbox 0x00088960
void tick_runtime_anim_state(std::span<u8> state, RuntimeAnimStateContext& context); // Xbox 0x000886B0
}
