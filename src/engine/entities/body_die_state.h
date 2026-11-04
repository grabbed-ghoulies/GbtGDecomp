#pragma once
#include "body_state_fields.h"

#include "body_action_selection.h"
#include "actor_clock.h"
#include "body_animation_setup.h"
#include "gbtg/actor_runtime.h"
#include "gbtg/retail_field.h"

#include <array>
#include <functional>

namespace gbtg
{
// Factory type 0x62 (getter 0x0008C1E0, vtable 0x00505720; named entityStateDie in the type table): the body plays
// a named animation from a random point near its end, optionally facing the view or the mind's target, and takes
// the next key when the animation slot finishes. +0x14/+0x18 are the empty 0x001628B0 and +0x1C is 0x0002B620.
// State +8 is the body, +0xC the next key and +0x10 the state clock (`body_state_field`).
namespace die_state_parameter
{
constexpr RetailField<u32, 0x0C> body_bit{};
constexpr RetailField<u32, 0x10> animation{}; // the animation's name, an XBE string
constexpr RetailField<u32, 0x14> moving_key{};
constexpr RetailField<u32, 0x18> faces_view{};
constexpr RetailField<u32, 0x1C> settles{};
constexpr u32 kSize = 0x20;
static_assert(retail_fits(kSize, settles));
} // namespace die_state_parameter
struct DieState
{
    BodyStateBase base{}; // +0
    std::array<char, 0x40> animation{}; // +0x18, the animation's name, copied in
    u32 body_bit = 0; // +0x58, raised while the state lasts
    u32 moving_key = 0; // +0x5C, the body's moving action (vtable +0x40) is the next key while it moves
    u32 faces_view = 0; // +0x60, the body turns toward the view
    u32 settles = 0; // +0x64, the body's turn settles each tick (0x00134720)
    u32 target = 0; // +0x68, the mind's target at entry; message 0x7C for it clears it
};
static_assert(sizeof(DieState) == 0x6C);
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
bool initialize_runtime_die_state(DieState& state, std::span<const u8> parameters,
    RuntimeDieStateContext& context); // Xbox 0x0008C240
// Starts the state's animation (ECX body, EAX state) at a random 0.9..1.1 of its slot's duration.
void start_runtime_die_animation(DieState& state, RuntimeDieStateContext& context); // Xbox 0x0008C4F0
void destroy_runtime_die_state(DieState& state, RuntimeDieStateContext& context); // Xbox 0x0008C2D0
void message_runtime_die_state(DieState& state, std::span<const u8> message); // Xbox 0x0008C4D0
void tick_runtime_die_state(DieState& state, RuntimeDieStateContext& context); // Xbox 0x0008C310
}
