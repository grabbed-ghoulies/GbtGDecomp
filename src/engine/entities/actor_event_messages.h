#pragma once

#include "actor_scare_requests.h"
#include "actor_trail_requests.h"
#include "../audio/event_voices.h"
#include "../graphics/actor_effect_stream.h"

namespace gbtg
{
// An animation event (0x0003A380 runs it by kind; the fields after +0x10 depend on the kind). See
// docs/RETAIL_LAYOUTS.md.
namespace animation_event_field
{
constexpr RetailField<float, 0x04> begin{};
constexpr RetailField<float, 0x08> end{};
constexpr RetailField<u16, 0x0C> kind{};
} // namespace animation_event_field

// Kind 4: set the shader bits while the event runs, clear them after.
namespace shader_bits_event
{
constexpr RetailField<u16, 0x10> slot{};
constexpr RetailField<u32, 0x14> mask{};
} // namespace shader_bits_event

// Kinds 0xC, 0x11, 0x26 and 0x27 (an effect stream, perhaps with a sound), 0x24 (a stream alone), 0x29 (a camera
// shake) and 0x2A (an object): a resource named at +0x10.
namespace effect_event
{
constexpr RetailField<std::array<char, 0x80>, 0x10> name{};
constexpr RetailField<u32, 0x90> binding{}; // kind 0x11: its low half is a locator; 0x29: set, the actor's scale
constexpr RetailField<u32, 0x94> timed{}; // the effect lasts the event (0x0003A9A0)
constexpr RetailField<u32, 0x98> scaled{};
constexpr RetailField<u32, 0x9C> include_euler{};
constexpr RetailField<float, 0xA0> chance{};
constexpr RetailField<u32, 0xA4> sound_slot{};
constexpr RetailField<std::array<char, 0x80>, 0xA8> sound{}; // empty for none
} // namespace effect_event

// Kind 0x16: a camera effect at the actor.
namespace camera_effect_event
{
constexpr RetailField<u32, 0x10> effect{};
constexpr RetailField<float, 0x14> size{};
constexpr RetailField<u32, 0x18> unk_18{}; // copied to the effect's +0x1C
constexpr RetailField<u32, 0x1C> positioned{}; // set: the effect sits at the actor, within low..high
constexpr RetailField<float, 0x20> low{};
constexpr RetailField<float, 0x24> high{};
constexpr RetailField<u32, 0x28> scaled{}; // the actor's scale instead of its radius
} // namespace camera_effect_event

// Kind 0x1F: stop a sound slot.
namespace stop_sound_event
{
constexpr RetailField<u32, 0x10> slot{};
} // namespace stop_sound_event

// Kind 0x25: the trail, at a rate, with an instance state set while it is laid.
namespace trail_event
{
constexpr RetailField<float, 0x10> rate{};
constexpr RetailField<u32, 0x14> sets_state{};
constexpr RetailField<u8, 0x18> state_slot{};
constexpr RetailField<u8, 0x19> state_value{};
} // namespace trail_event

// Kind 0x30: an audio event voice named at +0x14.
namespace voice_event
{
constexpr RetailField<std::array<char, 0x80>, 0x14> name{};
} // namespace voice_event

// Kind 0x31: an instance state.
namespace node_state_event
{
constexpr RetailField<u8, 0x10> slot{};
constexpr RetailField<u8, 0x14> value{};
} // namespace node_state_event

struct ActorEventContext
{
    RuntimeActorStartupState& state;
    ActorEffectStreamContext& effects;
    ActorTrailContext& trails;
    ActorScareContext& scares;
    AudioEventVoices& voices;
    std::span<u32> shader_states;
    float& trail_timer; // actor +1DC
    float& scare_intensity; // raw event interpretation of actor +958
    float& scare_scale; // raw event interpretation of actor +95C
};
void dispatch_actor_animation_event(const ActorAnimationMessage& message,
    ActorEventContext& context); // Xbox 0x0003A380
}
