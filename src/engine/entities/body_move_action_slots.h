#pragma once
#include "actor_clock.h"
#include "gbtg/anim.h"
#include <functional>

namespace gbtg
{
// One evaluation of the 0x00103880 integrator callbacks: context, state and output vectors.
using RuntimeMotionDerivative = std::function<void(std::span<const u8> context, const Vec3& state,
    const Vec3& rate, Vec3& output)>;
using RuntimeMotionConstraint = std::function<void(std::span<const u8> context, const Vec3& state,
    Vec3& output)>;
// Two-stage step repeated steps times; zero steps or a zero step size copies the inputs.
void integrate_runtime_motion(const Vec3& state, const Vec3& rate, Vec3& state_out, Vec3& rate_out,
    std::span<const u8> context, float step, i32 steps, const RuntimeMotionDerivative& derivative,
    const RuntimeMotionConstraint& state_constraint,
    const RuntimeMotionConstraint& rate_constraint); // Xbox 0x00103880, EDX state, ECX rate, EBX rate out, ESI context
// context: target xyz +0, gain +0xC.
void runtime_motion_toward_target(std::span<const u8> context, const Vec3& state, const Vec3& rate,
    Vec3& output); // Xbox 0x00140700
// context: target xyz +0; each output component settles on the target when it overshoots or lands within 1e-5.
void runtime_motion_settle_target(std::span<const u8> context, const Vec3& state, Vec3& output); // Xbox 0x00140770
// Actor +0x3C velocity eases toward body +0x318 at rate body +0x334 over DAT_004312E0 steps of 1/300.
void ease_runtime_body_velocity(std::span<const u8> body, Actor& actor, const i32& steps); // Xbox 0x000803B0, EAX body
// Move action type 0x5C: vtable +0x10 and +0x14.
void tick_runtime_velocity_action(std::span<u8> action, std::span<const u8> body, Actor& actor,
    const RuntimeActorClock& clock, const i32& steps); // Xbox 0x00080340
void ease_runtime_planar_velocity(Vec3& velocity, const Vec3& target, float gain,
    const i32& steps); // Xbox 0x00140660
void update_runtime_gravity_velocity(std::span<const u8> body, Actor& actor,
    const RuntimeActorClock& clock, const i32& steps); // Xbox 0x00133E50
void tick_runtime_gravity_action(std::span<u8> action, std::span<const u8> body, Actor& actor,
    const RuntimeActorClock& clock, const i32& steps); // Xbox 0x0007FD70
void step_runtime_body_motion(std::span<u8> body, Actor& actor, const ActorAnimationState& animation,
    const RuntimeActorClock& clock); // Xbox 0x0002AD30 (0x000803A0 jumps here)
// Eases `value` toward `target` with `gain` over DAT_004312E0 steps of 1/1200 (EAX target, EDX value).
void ease_runtime_vector_toward(Vec3& value, const Vec3& target, float gain, const i32& steps); // Xbox 0x001405E0

// State action type 0x5D, entityStateAnim2Stand (vtable 0x005054D0): the camera body's stand state.
// Action +0xC elapsed, +0x10 reset, +0x14 focus, +0x20 focus rate. Action +8 is the body; body +8 the actor.
struct RuntimeStandActionContext
{
    std::span<u8> body;
    Actor& actor; // Body +8.
    std::span<const u8> mind; // Actor +0x958: +0xC target, +0x70 eye.
    const RuntimeActorClock& clock;
    const i32& steps; // DAT_004312E0.
    std::span<const u8> tunables; // DAT_004275BC..DAT_004275CF.
    std::function<const Actor*(u32)> actor_at; // A target actor by address.
    // 0x0013CE80: output, from, to, target actor, range, near, far.
    std::function<void(Vec3&, const Vec3&, const Vec3&, u32, float, float, float)> aim;
};
void message_runtime_stand_action(std::span<u8> action, std::span<const u8> message,
    RuntimeStandActionContext& context); // Xbox 0x0007FE70
void follow_runtime_stand_focus(std::span<u8> action, RuntimeStandActionContext& context); // Xbox 0x000800A0, EDI body
void tick_runtime_stand_action(std::span<u8> action, RuntimeStandActionContext& context); // Xbox 0x0007FEC0
void step_runtime_stand_action(std::span<u8> action, RuntimeStandActionContext& context,
    const ActorAnimationState& animation); // Xbox 0x0007FFB0
}
