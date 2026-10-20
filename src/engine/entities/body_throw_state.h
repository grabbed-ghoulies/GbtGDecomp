#pragma once

#include "body_state_actions.h"
#include "actor_clock.h"
#include "player_attachment.h"
#include "../frontend/dialogue_input.h"

namespace gbtg
{
// Factory type 0x8B (getter 0x000A37F0, vtable 0x00505C40): the body throws what it holds at body +0x4C. The
// +0x14 / +0x18 slots are the empty 0x001628B0 and +0x1C is 0x0002B620. State +8 is the body.
//
// State words: +0x18 the actor's euler at entry, +0x24 the aim point, +0x30 the (smoothed) launch point, +0x3C set once
// +0x30 is seeded, +0x40 the near range (parameters +0x14), +0x44 set once thrown, +0x48 turn toward the aim
// (parameters +0x20), +0x4C smooth the launch point (+0x24), +0x50 settle the turn (+0x28), +0x54 the aim angle offset
// (parameters +0x18 degrees), +0x58 the aim mode (+0xC: 0..2 at the target's height, 3 the right stick, 4 at the
// weapon's height), +0x5C the spread (+0x10), +0x60 the next key (+0x1C) and +0x64 set when the animation ends.
struct RuntimeThrowStateContext
{
    std::span<u8> body;
    Actor& actor; // Body +8.
    std::span<const u8> actor_bytes; // +0xCDC, +0x93C.
    RuntimeBodyBitStorage& bits; // Body +0x15C.
    ActorAnimationState& animation; // Actor +0x50.
    const RuntimeActorClock& clock;
    entity_detail::ListHead& live_attachments; // Actor +0x8F4; its next (actor +0x8F8) is the first record.
    RuntimePlayerAttachment& attachment; // Body +0x48 / +0x4C / +0x14 and the action views.
    std::span<const u8> hit; // Body +0x84's record, or empty.
    std::span<const u8> target; // Actor +0x958 -> +0x14's target record, or empty.
    std::function<const Actor&(u32 address)> actor_at; // The target record's +0x10 actor.
    std::function<u16(u32 slot)> body_slot_key; // Body vtable +0x20.
    const RuntimeDialogueInputDevice* own_input; // Actor +0x960's device, or null.
    const RuntimeDialogueInputState& input_state;
    u32& random; // DAT_0054DD7C.
};
void throw_state_object_sizes(ObjectSizes& output); // Xbox 0x000A37D0
bool initialize_runtime_throw_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimeThrowStateContext& context); // Xbox 0x000A3850
void destroy_runtime_throw_state(std::span<u8> state, RuntimeThrowStateContext& context); // Xbox 0x000A3910
// Turns the actor toward the launch point, easing +0x30 toward +0x24 when +0x4C is set (EAX state).
void turn_runtime_throw_state(std::span<u8> state, RuntimeThrowStateContext& context); // Xbox 0x000A3D50
// The aim 20 ahead of the actor along the right stick, or along its yaw (ECX actor, EAX body, EBX the aim).
void aim_runtime_throw_by_stick(Vec3& aim, RuntimeThrowStateContext& context); // Xbox 0x000A3E80
// Spreads the launch point sideways by a random amount within +-`spread` (ECX actor).
void spread_runtime_throw(float spread, Vec3& point, const Actor& actor, u32& random); // Xbox 0x000A3FE0
// The target's position, led by its velocity over body +0x7C unless that turns the throw away (EAX actor, EBX the
// target record).
void lead_runtime_throw_target(Vec3& aim, RuntimeThrowStateContext& context); // Xbox 0x000A4100
// The horizontal distance from the actor to `point` (ESI actor, EBX point).
long double runtime_actor_flat_distance(const Actor& actor, const Vec3& point); // Xbox 0x000413F0
void tick_runtime_throw_state(std::span<u8> state, RuntimeThrowStateContext& context); // Xbox 0x000A3990
void message_runtime_throw_state(std::span<const u8> state, std::span<const u8> message,
    RuntimeThrowStateContext& context); // Xbox 0x000A3D20
}
