#pragma once

#include "actor_clock.h"
#include "body_state_fields.h"
#include "player_attachment.h"
#include "gbtg/object_factory.h"

#include <functional>

namespace gbtg
{
// Factory type 0x71 (getter 0x00095550, vtable 0x00505920; the factory table's row 0x70 names it entityStateGrabbed,
// the name body_grabbed_state.h gives type 0x70) over the body state (type 9): the body is held by another (the
// vampire's bite, type 0x61, through message 0x3C). It drops what it holds and raises its latches, holds (+0x258),
// +0x30 count, +0x309..+0x30C counts and bit block byte 4 bit 1 until it is destroyed; the tick only keeps the
// elapsed time. Sizes are actorMove's 0x00095530 (0x18 / 0xC). The message and the +0x14 / +0x18 slots are the empty
// 0x001628B0 and +0x1C is 0x0002B620. State +8 is the body.
struct RuntimeGrabbedHoldStateContext
{
    std::span<u8> body;
    Actor& actor; // Body +8.
    EntityBitBlock& bits; // Body +0x15C.
    RuntimePlayerAttachment& attachment; // 0x0001B810's views.
    entity_detail::LatchNode*& latches; // DAT_00511AA4.
    const RuntimeActorClock& clock;
    std::function<void(std::span<const u32>)> body_message; // Body vtable +0xC.
};
bool initialize_runtime_grabbed_hold_state(std::span<u8> state,
    RuntimeGrabbedHoldStateContext& context); // Xbox 0x000955B0
void destroy_runtime_grabbed_hold_state(std::span<u8> state,
    RuntimeGrabbedHoldStateContext& context); // Xbox 0x00095640
void tick_runtime_grabbed_hold_state(std::span<u8> state,
    RuntimeGrabbedHoldStateContext& context); // Xbox 0x000956C0
// Type 0x71 (base 9) with its lifecycle; `context_of` gives a state's views by its body address (state +8).
void install_runtime_grabbed_hold_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeGrabbedHoldStateContext&(u32 body_address)> context_of);
}
