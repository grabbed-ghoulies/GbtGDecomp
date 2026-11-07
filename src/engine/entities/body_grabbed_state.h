#pragma once
#include "body_state_fields.h"
#include "body_collision.h"
#include "body_pickup_state.h"
#include "body_hit_state.h"
#include "body_timer_requests.h"

namespace gbtg
{
// entityStateGrabbed (factory type 0x70, getter 0x000942F0, vtable 0x005058E0) over actorMove (type 9). +0xC and
// +0x14/+0x18 are the empty 0x000D4179 case and +0x1C is 0x0002B620. The body holds still (+0x30, +0x309..+0x30C,
// its live records' +0x15, +0x6D) until its animation passes the release mark (+0x18) or ends and the stick moves.
// State +0x10 is the state clock (`BodyStateBase`).
namespace grabbed_state_parameter
{
constexpr RetailField<float, 0x0C> release_mark{};
constexpr RetailField<u32, 0x10> holds_latches{};
constexpr RetailField<u32, 0x14> body_bit{};
constexpr u32 kSize = 0x18;
static_assert(retail_fits(kSize, body_bit));
} // namespace grabbed_state_parameter
struct GrabbedState
{
    BodyStateBase base{}; // +0
    float release_mark = 0.f; // +0x18, the animation position that drops move_hold
    u32 move_hold = 0; // +0x1C, the state holds body +0x30
    u32 holds_latches = 0; // +0x20, the latches are held (0x00027BE0); the destructor drops body +0x7E5
    u32 body_bit = 0; // +0x24, raised while the state lasts
};
static_assert(sizeof(GrabbedState) == 0x28);
struct RuntimeGrabbedStateContext
{
    RuntimePickupStateContext& pickup; // Body, actor, bits, animation, clock, live records, input, actor message.
    RuntimeBodyRecoveryAction& recovery; // 0x00134060.
    RuntimeBodyHoldTimerContext& hold; // 0x00027BE0.
    RuntimeBodyCollisionSphere* spheres; // Body +0x710; the count is body +0x714.
};

void grabbed_state_object_sizes(ObjectSizes& output); // Xbox 0x000942D0
bool initialize_runtime_grabbed_state(GrabbedState& state, std::span<const u8> parameters,
    RuntimeGrabbedStateContext& context); // Xbox 0x00094350
void destroy_runtime_grabbed_state(GrabbedState& state, RuntimeGrabbedStateContext& context); // Xbox 0x00094450
void tick_runtime_grabbed_state(GrabbedState& state, RuntimeGrabbedStateContext& context); // Xbox 0x00094530
}
