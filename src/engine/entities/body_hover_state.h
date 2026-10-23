#pragma once

#include "actor_clock.h"
#include "body_action_selection.h"
#include "body_state_fields.h"
#include "../frontend/dialogue_input.h"
#include "gbtg/anim.h"
#include "gbtg/object_factory.h"

#include <functional>

namespace gbtg
{
// Factory type 0x77, entityStateHover (getter 0x00097B20, vtable 0x005059C0) over the body state (type 9): the body
// hovers in place playing the parameters' animation (+0x14), its turn limits widened (+0x71C 60 degrees, +0x720 130,
// +0x724 6), facing the mind's chosen actor when it has one and, with +0x10 set, snapping its heading round once that
// actor is more than 50 degrees off. Pushing the left stick (or the third pair, 0x00108AB0) ends it: the next action
// is the parameters' +0xE when the mind has chosen an actor, else +0xC. The message and the +0x14 / +0x18 slots are
// the empty 0x001628B0 and +0x1C is 0x0002B620. State +8 is the body.
namespace hover_state_parameter
{
constexpr RetailField<u16, 0x0C> next{};
constexpr RetailField<u16, 0x0E> next_with_target{};
constexpr RetailField<u32, 0x10> turns{};
constexpr RetailField<u32, 0x14> animation{}; // an XBE string
constexpr u32 kSize = 0x18;
static_assert(retail_fits(kSize, animation));
} // namespace hover_state_parameter
namespace hover_state_field
{
constexpr RetailField<u16, 0x18> next{}; // parameters +0xC
constexpr RetailField<u16, 0x1A> next_with_target{}; // parameters +0xE
constexpr RetailField<u32, 0x1C> turns{}; // parameters +0x10
constexpr RetailField<u32, 0x20> animation{}; // set when the animation's record was found
constexpr u32 kSize = 0x24;
static_assert(retail_fits(kSize, animation));
} // namespace hover_state_field
struct RuntimeHoverStateContext
{
    std::span<u8> body;
    Actor& actor; // Body +8.
    EntityBitBlock& bits; // Body +0x15C.
    RuntimeBodyActionSelection& actions; // 0x0001AED0's lookup and 0x0001AFA0's start.
    const RuntimeActorClock& clock;
    ActorAnimationState& animation; // Actor +0x50.
    std::span<const u8> mind; // Actor +0x958; +0xC the chosen actor, +0x20 its flag.
    const RuntimeDialogueInputDevice* input; // The tick's second argument (actor +0x960 or zero).
    const RuntimeDialogueInputState& input_state;
    std::function<Actor*(u32)> actor_at; // A scene actor by retail address.
    std::function<const char*(u32)> image_string; // An XBE string, or null for 0.
};
void hover_state_object_sizes(ObjectSizes& output); // Xbox 0x00097B00
bool initialize_runtime_hover_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimeHoverStateContext& context); // Xbox 0x00097B80
void destroy_runtime_hover_state(std::span<const u8> state, RuntimeHoverStateContext& context); // Xbox 0x00097C60
void tick_runtime_hover_state(std::span<u8> state, RuntimeHoverStateContext& context); // Xbox 0x00097CF0
// The stick ends the hover: the next action by whether the mind has chosen an actor.
void check_runtime_hover_stick(std::span<u8> state, RuntimeHoverStateContext& context); // Xbox 0x00097DC0, ESI input, EBX state
// The body faces the point at body +0x3C, its heading set when that point is more than 50 degrees off.
void turn_runtime_hover_state(RuntimeHoverStateContext& context); // Xbox 0x00097E60, EAX actor
// Type 0x77 (base 9) with its lifecycle; `context_of` gives a state's views by its body address (state +8).
void install_runtime_hover_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeHoverStateContext&(u32 body_address)> context_of);
}
