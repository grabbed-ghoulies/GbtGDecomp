#pragma once
#include "body_state_actions.h"
#include "actor_clock.h"
#include "../frontend/dialogue_input.h"

namespace gbtg
{
// entityStateStruggle (factory type 0x88, getter 0x000A2B30, vtable 0x00505BE0) over actorMove (type 9). +0xC and
// +0x14/+0x18 are the empty 0x000D4179 case and +0x1C is 0x0002B620. State bytes: +0x18/+0x1A input and idle keys,
// +0x1C moving-key flag, +0x20 settle flag, +0x24/+0x28 the in-band and out-of-band animations (found or not; the
// records stay on the host), +0x2C band timer, +0x30 started, +0x34 band mode (1 in band, 2 out), +0x38 turn limit
// in degrees, +0x3C heading offset, +0x40..+0x48 the body's turn rates while struggling.
void struggle_state_object_sizes(ObjectSizes& output); // Xbox 0x000A2B10
bool initialize_runtime_struggle_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimeBodyStateContext& context); // Xbox 0x000A2B90
void destroy_runtime_struggle_state(std::span<u8> state, RuntimeBodyStateContext& context); // Xbox 0x000A2D60

struct RuntimeStruggleTick
{
    std::span<u8> body;
    Actor& actor; // Body +8.
    std::span<const u8> actor_bytes; // +0x18 facing.
    const EntityBitBlock& flags; // Body +0x15C.
    RuntimeBodyActionSelection& actions;
    const RuntimeActorClock& clock;
    const RuntimeDialogueInputDevice& input;
    const RuntimeDialogueInputState& input_state;
    std::span<const u8> mind; // Actor +0x958; empty when there is none. +0xC target, +0x20 alert.
    std::function<u16()> moving_action; // Body vtable +0x40.
    std::function<Vec3(u32)> target_position; // 0x000412A0 on the actor at a retail address.
};
void turn_runtime_struggle_body(bool targeted, std::span<u8> state,
    RuntimeStruggleTick& context); // Xbox 0x000A2F90, EAX targeted, ESI body, EDI state
void update_runtime_struggle_animation(std::span<u8> state,
    RuntimeStruggleTick& context); // Xbox 0x000A3070, EAX actor, ECX body, EDI state
void select_runtime_struggle_key(std::span<u8> state, RuntimeStruggleTick& context); // Xbox 0x000A2EB0, EBX state
void tick_runtime_struggle_state(std::span<u8> state, RuntimeStruggleTick& context); // Xbox 0x000A2DB0
}
