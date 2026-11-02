#pragma once

#include "body_action_selection.h"
#include "body_animation_setup.h"
#include "actor_clock.h"
#include "../frontend/dialogue_input.h"
#include "gbtg/actor_runtime.h"
#include "gbtg/retail_field.h"
#include "gbtg/sound_runtime.h"

#include <array>
#include <functional>

namespace gbtg
{
// Factory type 0x5E (getter 0x00088520, vtable 0x005056A0): a body state that plays an optional animation and
// sound, can turn the body toward the mind's target, and takes its next key on a timer, at the animation's end, or
// once the animation passes a point and the stick is pushed. +0x14/+0x18 are the empty 0x001628B0 and +0x1C is
// 0x0002B620. State +8 is the body, +0xC the next key and +0x10 the state clock (`body_state_field`).
namespace anim_state_parameter
{
constexpr RetailField<u16, 0x0C> next_key{};
constexpr RetailField<u32, 0x10> body_bit{};
constexpr RetailField<u32, 0x14> faces_target{};
constexpr RetailField<float, 0x18> heading_offset{};
constexpr RetailField<float, 0x1C> stick_point{};
constexpr RetailField<float, 0x20> time{}; // zero or less: the state ends with its animation instead
constexpr RetailField<std::array<char, 0x80>, 0x24> sound{}; // an inline cue name; empty plays none
constexpr RetailField<u32, 0xA4> settles{};
constexpr RetailField<u32, 0xA8> animation{}; // the animation's name, an XBE string, or 0 for none
constexpr RetailField<u32, 0xAC> carries_velocity{};
constexpr u32 kSize = 0xB0;
static_assert(retail_fits(kSize, carries_velocity));
} // namespace anim_state_parameter
namespace anim_state_field
{
constexpr RetailField<u16, 0x18> next_key{}; // 0 takes body vtable +0x20's key
constexpr RetailField<u32, 0x1C> body_bit{}; // raised while the state lasts
constexpr RetailField<float, 0x20> time_left{};
constexpr RetailField<u32, 0x24> timed{}; // the state ends when time_left runs out, not with the animation
constexpr RetailField<u32, 0x28> sound_started{};
constexpr RetailField<u32, 0x2C> faces_target{}; // the body turns toward the mind's target
constexpr RetailField<float, 0x30> heading_offset{}; // added to the angle toward the target
constexpr RetailField<float, 0x34> stick_point{}; // past this animation position a pushed stick ends the state
constexpr RetailField<u32, 0x38> carries_velocity{}; // body +0x318 takes the actor's velocity (+0x3C)
constexpr RetailField<u32, 0x3C> settles{}; // the body's turn settles each tick (0x00134720)
constexpr u32 kSize = 0x40;
static_assert(retail_fits(kSize, settles));
} // namespace anim_state_field
struct RuntimeAnimStateContext
{
    std::span<u8> body;
    Actor& actor; // Body +8.
    RuntimeBodyBitStorage& bits; // Body +0x15C.
    ActorAnimationState& animation; // Actor +0x50.
    const RuntimeActorClock& clock;
    RuntimeBodyActionSelection& actions; // 0x0001AF10 / 0x00027650 views.
    RuntimeActorSoundQueues& sounds;
    float stick_threshold; // DAT_0032B22C.
    std::function<const char*(u32 address)> string_at; // An XBE string the parameters name.
    std::function<u32()> mind_target; // Actor +0x958 +0xC, or 0 when actor +0x958 is 0.
    std::function<const Actor*(u32 address)> actor_at;
    std::function<u16(u32 slot)> body_slot_key; // Body vtable +0x20.
    const RuntimeDialogueInputDevice* input = nullptr; // The tick's second argument (actor +0x960 or zero).
    const RuntimeDialogueInputState* input_state = nullptr;
};

void anim_state_object_sizes(ObjectSizes& output); // Xbox 0x00088500
bool initialize_runtime_anim_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimeAnimStateContext& context); // Xbox 0x00088580
void destroy_runtime_anim_state(std::span<u8> state, RuntimeAnimStateContext& context); // Xbox 0x00088660
void message_runtime_anim_state(std::span<u8> state, std::span<const u8> message,
    RuntimeAnimStateContext& context); // Xbox 0x00088960
void tick_runtime_anim_state(std::span<u8> state, RuntimeAnimStateContext& context); // Xbox 0x000886B0
}
