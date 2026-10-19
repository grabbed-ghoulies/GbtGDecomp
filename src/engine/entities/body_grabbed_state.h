#pragma once
#include "body_pickup_state.h"
#include "body_hit_state.h"
#include "body_timer_requests.h"

namespace gbtg
{
// entityStateGrabbed (factory type 0x70, getter 0x000942F0, vtable 0x005058E0) over actorMove (type 9). +0xC and
// +0x14/+0x18 are the empty 0x000D4179 case and +0x1C is 0x0002B620. The body holds still (+0x30, +0x309..+0x30C,
// its live records' +0x15, +0x6D) until its animation passes the release mark (+0x18) or ends and the stick moves.
//
// State words: +0x10 the state clock, +0x18 the release mark (parameters +0xC), +0x1C the +0x30 hold, +0x20
// parameters +0x10 (0x00027BE0's hold), +0x24 a body bit (parameters +0x14).
struct RuntimeGrabbedStateContext
{
    RuntimePickupStateContext& pickup; // Body, actor, bits, animation, clock, live records, input, actor message.
    RuntimeBodyRecoveryAction& recovery; // 0x00134060.
    RuntimeBodyHoldTimerContext& hold; // 0x00027BE0.
};

void grabbed_state_object_sizes(ObjectSizes& output); // Xbox 0x000942D0
bool initialize_runtime_grabbed_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimeGrabbedStateContext& context); // Xbox 0x00094350
void destroy_runtime_grabbed_state(std::span<u8> state, RuntimeGrabbedStateContext& context); // Xbox 0x00094450
void tick_runtime_grabbed_state(std::span<u8> state, RuntimeGrabbedStateContext& context); // Xbox 0x00094530
}
