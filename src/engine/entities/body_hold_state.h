#pragma once

#include "body_state_actions.h"
#include "body_hit_state.h"
#include "actor_clock.h"

namespace gbtg
{
// Type 0x8A (getter 0x000A34C0, vtable 0x00505C20): a timed hold before the body's next goal action. The message and
// the +0x14 / +0x18 slots are the empty 0x001628B0 and +0x1C is 0x0002B620. State +0x18 is the timer, +0x1C the
// settle flag (the state's own +0xC word at construction) and +0x20 an optional body bit.
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
