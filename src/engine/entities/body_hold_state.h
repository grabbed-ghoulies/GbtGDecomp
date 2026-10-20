#pragma once

#include "body_state_actions.h"
#include "body_hit_state.h"
#include "actor_clock.h"
#include "body_state_fields.h"

namespace gbtg
{
// Type 0x8A (getter 0x000A34C0, vtable 0x00505C20): a timed hold before the body's next goal action. The message and
// the +0x14 / +0x18 slots are the empty 0x001628B0 and +0x1C is 0x0002B620. State +0x18 is the timer, +0x1C the
// settle flag (the state's own +0xC word at construction) and +0x20 an optional body bit.
// The hold state's own fields at their retail offsets (0x24 bytes, 0x000A34A0), after the body-state base
// (body_state_field).
namespace hold_state_field
{
// Seconds left: body +0x240 at construction, plus (1 - body +0x16C) * 0.3 (DAT_0032B168). The goal action
// follows when it reaches 0.
constexpr RetailField<float, 0x18> timer{};
// The base's +0xC word at construction; set, each tick settles the body's turn (0x00134720).
constexpr RetailField<u32, 0x1C> settle_turn{};
constexpr RetailField<u32, 0x20> body_bit{}; // a body bit (body +0x15C) held set while the state runs; 0 for none
constexpr u32 kSize = 0x24;
static_assert(retail_fits(kSize, body_bit));
} // namespace hold_state_field
void hold_state_object_sizes(ObjectSizes& output); // Xbox 0x000A34A0
bool initialize_runtime_hold_state(std::span<u8> state, RuntimeBodyStateContext& context); // Xbox 0x000A3520
void destroy_runtime_hold_state(std::span<const u8> state, RuntimeBodyStateContext& context); // Xbox 0x000A35E0
struct RuntimeHoldStateTick
{
    std::span<u8> body;
    Actor& actor; // Body +8.
    u32 actor_address;
    const RuntimeActorClock& clock;
    RuntimeBodyRecoveryAction& recovery; // 0x00134060 and the 0x001346B0 views.
};
void tick_runtime_hold_state(std::span<u8> state, RuntimeHoldStateTick& context); // Xbox 0x000A3670
}
