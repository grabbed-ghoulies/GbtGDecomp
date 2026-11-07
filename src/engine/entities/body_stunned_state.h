#pragma once

#include "body_state_actions.h"
#include "body_state_fields.h"
#include "actor_clock.h"
#include "gbtg/sound_runtime.h"

#include <array>
#include <functional>

namespace gbtg
{
struct LiveScript;
struct RuntimeBodyActionMessages;
// Factory type 0x89 (getter 0x000A31A0, vtable 0x00505C00) over the body state (type 9): the body is held. It takes
// the four input counters (+0x309 .. +0x30C) and +0x30, drops the mind's target and the mind's type 3 event, and
// switches to shape mode 2, with the parameters' sound on slot 4. Message 0x43 starts a half-second wait, after which
// the body's own action key comes next; message 0x53 answers through 0x00023890 with ECX 0. The +0x14 / +0x18 slots
// are the empty 0x001628B0 and +0x1C is 0x0002B620. State +8 is the body.
namespace stunned_state_parameter
{
constexpr RetailField<std::array<char, 0x80>, 0x0C> sound{}; // queued on slot 4 (0x0012A260) when not empty
constexpr u32 kSize = 0x8C;
static_assert(retail_fits(kSize, sound));
} // namespace stunned_state_parameter
struct StunnedState
{
    BodyStateBase base{}; // +0
    u8 shape_mode = 0; // +0x18, the shape mode 0x0001FEB0 replaced, restored on release
    std::array<u8, 3> unk_19{}; // +0x19, padding
    float wait = 0.f; // +0x1C, seconds left after message 0x43 (DAT_0032B0F4)
};
static_assert(sizeof(StunnedState) == 0x20);
struct RuntimeStunnedStateContext
{
    std::span<u8> body;
    Actor& actor; // Body +8.
    RuntimeBodyStateContext& shape; // 0x0001FEB0's views.
    const RuntimeActorClock& clock;
    RuntimeActorSoundQueues& sounds;
    std::span<u8> mind; // Actor +0x958; empty when the actor has none.
    LiveScript* mind_events; // Mind +0x24's anchor; its next (mind +0x28) is the first event.
    std::function<void(std::span<const u32>)> actor_message; // Mind +8 (the actor) vtable +0xC.
    std::function<void(u32 address, std::span<u32> message)> message; // An object's vtable +0xC.
    std::function<u16()> action_key; // Body vtable +0x20.
    RuntimeBodyActionMessages& actions; // 0x00024B20's views, for 0x00023890.
};
void stunned_state_object_sizes(ObjectSizes& output); // Xbox 0x000A3180
bool initialize_runtime_stunned_state(StunnedState& state, std::span<const u8> parameters,
    RuntimeStunnedStateContext& context); // Xbox 0x000A3200
void destroy_runtime_stunned_state(StunnedState& state, RuntimeStunnedStateContext& context); // Xbox 0x000A3300
void tick_runtime_stunned_state(StunnedState& state, RuntimeStunnedStateContext& context); // Xbox 0x000A3380
void message_runtime_stunned_state(StunnedState& state, std::span<u8> message,
    RuntimeStunnedStateContext& context); // Xbox 0x000A3460
// Type 0x89 (base 9) with its lifecycle; `context_of` gives a state's views by its body address (state +8).
void install_runtime_stunned_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeStunnedStateContext&(u32 body_address)> context_of);
}
