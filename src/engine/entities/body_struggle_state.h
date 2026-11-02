#pragma once
#include "body_state_actions.h"
#include "actor_clock.h"
#include "../frontend/dialogue_input.h"
#include "gbtg/retail_field.h"

#include <array>

namespace gbtg
{
// entityStateStruggle (factory type 0x88, getter 0x000A2B30, vtable 0x00505BE0) over actorMove (type 9). +0xC and
// +0x14/+0x18 are the empty 0x000D4179 case and +0x1C is 0x0002B620. State +0xC is the next key and +0x10 the
// state clock (`body_state_field`). The in-band and out-of-band animation records stay on the host; the state only
// says whether each was found.
namespace struggle_state_parameter
{
constexpr RetailField<u16, 0x0C> input_key{};
constexpr RetailField<u16, 0x0E> idle_key{};
constexpr RetailField<u32, 0x10> moving_key{};
constexpr RetailField<u32, 0x14> settles{};
constexpr RetailField<u32, 0x18> in_band{}; // the animation's name, an XBE string
constexpr RetailField<u32, 0x1C> out_of_band{}; // the animation's name, an XBE string
constexpr RetailField<float, 0x20> turn_limit{}; // degrees
constexpr RetailField<float, 0x24> heading_offset{}; // degrees
// The body's turn rates while struggling: two in degrees, the third as is; zero takes the default.
constexpr RetailField<std::array<float, 3>, 0x28> turn_rates{};
constexpr u32 kSize = 0x34;
static_assert(retail_fits(kSize, turn_rates));
} // namespace struggle_state_parameter
namespace struggle_state_field
{
constexpr RetailField<u16, 0x18> input_key{}; // the next key while the stick is pushed
constexpr RetailField<u16, 0x1A> idle_key{}; // the next key while the mind has no target
constexpr RetailField<u32, 0x1C> moving_key{}; // the body's moving action (vtable +0x40) is the next key while it moves
constexpr RetailField<u32, 0x20> settles{}; // the body's turn settles each tick (0x00134720)
constexpr RetailField<u32, 0x24> in_band_found{};
constexpr RetailField<u32, 0x28> out_of_band_found{};
constexpr RetailField<float, 0x2C> band_timer{}; // seconds before the in-band animation may return
constexpr RetailField<u32, 0x30> started{}; // the in-band animation has played
constexpr RetailField<u32, 0x34> band_mode{}; // 1 facing within 5 degrees of the heading, 2 outside it
constexpr RetailField<float, 0x38> turn_limit{}; // degrees; a larger turn toward the target snaps the heading
constexpr RetailField<float, 0x3C> heading_offset{}; // radians, added to the heading toward the body's point
constexpr RetailField<std::array<float, 3>, 0x40> turn_rates{}; // copied into body +0x71C..+0x724
constexpr u32 kSize = 0x4C;
static_assert(retail_fits(kSize, turn_rates));
} // namespace struggle_state_field
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
