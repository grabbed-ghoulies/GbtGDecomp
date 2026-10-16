#pragma once

#include "body_action_selection.h"
#include "actor_clock.h"
#include "body_animation_setup.h"
#include "gbtg/actor_runtime.h"

#include <functional>

namespace gbtg
{
// Factory type 0x62 (getter 0x0008C1E0, vtable 0x00505720; named entityStateDie in the type table): the body plays
// a named animation from a random point near its end, optionally facing the view or the mind's target, and takes
// the next key when the animation slot finishes. +0x14/+0x18 are the empty 0x001628B0 and +0x1C is 0x0002B620.
// State +8 is the body.
//
// State words: +0xC the next key, +0x10 the state clock, +0x18 the animation name (parameters +0x10), +0x58 the body
// bit it raises (parameters +0xC), +0x5C take the body's damage key while moving (parameters +0x14), +0x60 face the
// view (parameters +0x18), +0x64 settle the body's turn (parameters +0x1C), +0x68 the mind's target when it began.
struct RuntimeDieStateContext
{
    std::span<u8> body;
    Actor& actor; // Body +8.
    RuntimeBodyBitStorage& bits; // Body +0x15C.
    ActorAnimationState& animation; // Actor +0x50.
    const RuntimeActorClock& clock;
    RuntimeBodyActionSelection& actions; // 0x0001AF10's views.
    u32& random; // DAT_0054DD7C.
    const Vec3& view_position; // DAT_005077A8 +0.
    std::function<const char*(u32 address)> string_at; // An XBE string the parameters name.
    std::function<u32()> mind_target; // Actor +0x958 +0xC, or 0 when actor +0x958 is 0.
    std::function<const Actor*(u32 address)> actor_at;
    std::function<u16(u32 slot)> body_slot_key; // Body vtable +0x20 / +0x40.
};

void die_state_object_sizes(ObjectSizes& output); // Xbox 0x0008C1C0
bool initialize_runtime_die_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimeDieStateContext& context); // Xbox 0x0008C240
// Starts the state's animation (ECX body, EAX state) at a random 0.9..1.1 of its slot's duration.
void start_runtime_die_animation(std::span<u8> state, RuntimeDieStateContext& context); // Xbox 0x0008C4F0
void destroy_runtime_die_state(std::span<u8> state, RuntimeDieStateContext& context); // Xbox 0x0008C2D0
void message_runtime_die_state(std::span<u8> state, std::span<const u8> message); // Xbox 0x0008C4D0
void tick_runtime_die_state(std::span<u8> state, RuntimeDieStateContext& context); // Xbox 0x0008C310
}
