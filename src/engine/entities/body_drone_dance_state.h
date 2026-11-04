#pragma once
#include "body_state_fields.h"

#include "actor_clock.h"
#include "body_action_selection.h"
#include "gbtg/actor_runtime.h"
#include "gbtg/object_factory.h"
#include "gbtg/retail_field.h"
#include "../navigation/occupancy.h"

#include <array>
#include <functional>

namespace gbtg
{
// Factory type 0x67, entityStateDroneDance (getter 0x0008E010, vtable 0x005057C0) over the body state (type 9): the
// body idles on its dance animation for one to two seconds, then glides to a random point of its room (actor
// +0xCA0) at 40 to 50 a second while turning to a random yaw, and idles again. Message 0x2D (with no reaction in
// the last five seconds) plays the reaction animation once, turned toward the sender; 0x7C from that actor forgets
// it. The +0x14 / +0x18 slots are the empty 0x001628B0 and +0x1C is 0x0002B620. State +8 is the body.
namespace drone_dance_state_parameter
{
constexpr RetailField<u32, 0x0C> dance{}; // An XBE string: the idle and glide animation.
constexpr RetailField<u32, 0x10> reaction{}; // An XBE string: the animation message 0x2D plays.
constexpr u32 kSize = 0x14;
static_assert(retail_fits(kSize, reaction));
} // namespace drone_dance_state_parameter
struct DroneDanceState
{
    BodyStateBase base{}; // +0
    u32 phase = 0; // +0x18, 1 idle, 2 glide, 3 reaction
    float left = 0.f; // +0x1C, seconds left in the idle or glide
    float reaction_wait = 0.f; // +0x20, five seconds after a reaction (DAT_0032B184)
    float glide_time = 0.f; // +0x24, the glide's whole length
    Vec3 velocity{}; // +0x28, copied to body +0x318 each tick
    Vec3 start_euler{}; // +0x34, the actor's world euler at the glide's start
    Vec3 target_euler{}; // +0x40, {0, random yaw, 0}
    u32 facing = 0; // +0x4C, the actor message 0x2D came from, or 0
    std::array<char, 0x40> dance{}; // +0x50, parameters +0xC's name
    std::array<char, 0x40> reaction{}; // +0x90, parameters +0x10's name
};
static_assert(sizeof(DroneDanceState) == 0xD0);
struct RuntimeDroneDanceStateContext
{
    std::span<u8> body;
    Actor& actor; // Body +8.
    ActorAnimationState& animation; // Actor +0x50.
    const RuntimeActorClock& clock;
    RuntimeBodyActionSelection& actions; // 0x0001AF10's views.
    u32& random; // DAT_0054DD7C.
    const OccupancyRecord* const& room; // Actor +0xCA0.
    std::function<const char*(u32 address)> string_at; // An XBE string the parameters name.
    std::function<const Actor*(u32 address)> actor_at; // State +0x4C's actor.
};
void drone_dance_state_object_sizes(ObjectSizes& output); // Xbox 0x0008DFF0
bool initialize_runtime_drone_dance_state(DroneDanceState& state, std::span<const u8> parameters,
    RuntimeDroneDanceStateContext& context); // Xbox 0x0008E070
void destroy_runtime_drone_dance_state(DroneDanceState& state,
    RuntimeDroneDanceStateContext& context); // Xbox 0x0008E120
void tick_runtime_drone_dance_state(DroneDanceState& state, RuntimeDroneDanceStateContext& context); // Xbox 0x0008E130
void message_runtime_drone_dance_state(DroneDanceState& state, std::span<const u32> message,
    RuntimeDroneDanceStateContext& context); // Xbox 0x0008E480
// Enters a phase: 1 idles for one to two seconds, 2 glides to a room point (or idles without one), 3 reacts.
void set_runtime_drone_dance_phase(DroneDanceState& state, u32 phase,
    RuntimeDroneDanceStateContext& context); // Xbox 0x0008E500, ECX state, EAX phase
// Type 0x67 (base 9) with its lifecycle; `context_of` gives a state's views by its body address (state +8).
void install_runtime_drone_dance_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeDroneDanceStateContext&(u32 body_address)> context_of);
}
