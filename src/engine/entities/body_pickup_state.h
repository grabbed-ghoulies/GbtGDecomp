#pragma once

#include "body_state_actions.h"
#include "player_attachment.h"
#include "actor_clock.h"
#include "actor_live_attachments.h"
#include "body_state_fields.h"
#include "../frontend/dialogue_input.h"
#include "gbtg/actor_runtime.h"

#include <functional>

namespace gbtg
{
struct Game;

// Factory type 0x7C (getter 0x0009DDA0, vtable 0x00505A60): the body picks up the weapon its mind chose (mind +0x18,
// set by 0x0007D210). The message and the +0x14 / +0x18 slots are the empty 0x001628B0 and +0x1C is 0x0002B620.
// State +8 is the body.
//
// State words: +0xC the next key, +0x10 the state clock, +0x18 the weapon taken, +0x1C..+0x24 the actor's position at
// entry, +0x28 the animation point after which the stick ends the state (parameters +0xC).
struct RuntimePickupStateContext
{
    std::span<u8> body;
    Actor& actor; // Body +8.
    std::span<const u8> actor_bytes;
    RuntimeBodyBitStorage& bits; // Body +0x15C.
    ActorAnimationState& animation; // Actor +0x50.
    const RuntimeActorClock& clock;
    entity_detail::ListHead& live_attachments; // Actor +0x8F4; its next (actor +0x8F8) is the first record.
    RuntimePlayerAttachment& attachment; // 0x0001B190's views.
    const Vec3& default_position; // DAT_0050DD80.
    const Vec3& default_euler; // DAT_0050DD74.
    float stick_threshold; // DAT_0032B22C.
    std::span<u8> mind; // Actor +0x958, or empty when it is 0.
    // Mind +0x18's weapon views (its bytes, actor, receiver and pose).
    std::function<RuntimePlayerAttachmentActor&(u32 address)> weapon_at;
    // 0x0003BE20 over the weapon's +0x8C callout records with callback 0, no options and value 0.
    std::function<void(u32 weapon, u32 id)> weapon_callouts;
    std::function<void(std::span<const u32>)> actor_message; // Actor vtable +0xC; zero-filled past the given words.
    std::function<u16(u32 slot)> body_slot_key; // Body vtable +0x20 / +0x2C.
    Game& game; // 0x00116810.
    const RuntimeDialogueInputDevice* input = nullptr; // The tick's second argument (actor +0x960 or zero).
    const RuntimeDialogueInputState* input_state = nullptr;
    // Type 0x7D: the weapon's vtable +0xC (its reply words are written back) and the body latches
    // (0x0001FC40 / 0x0001FD20, DAT_00511AA4).
    std::function<void(u32 weapon, std::span<const u32>)> weapon_message{};
    entity_detail::LatchNode** latches = nullptr;
    std::span<u8> actor_words{}; // The actor's bytes, for 0x0009E9F0's +0x314 store.
    std::span<const u8> address_zero{}; // What a null input word reads (0x0009E6B0's stick).
};

// The pick-up state's parameters (0x10 bytes) and own fields (0x2C bytes, 0x0009DD80), after the body-state base.
namespace pickup_state_parameter
{
constexpr RetailField<float, 0x0C> release_point{};
constexpr u32 kSize = 0x10;
static_assert(retail_fits(kSize, release_point));
} // namespace pickup_state_parameter
namespace pickup_state_field
{
constexpr RetailField<u32, 0x18> taken{}; // set once the animation passes the weapon's grab point (0x0009DF90)
constexpr RetailField<Vec3, 0x1C> start_position{}; // the actor's position at entry (0x000412A0)
constexpr RetailField<float, 0x28> release_point{}; // the animation point after which the stick ends the state
constexpr u32 kSize = 0x2C;
static_assert(retail_fits(kSize, release_point));
} // namespace pickup_state_field
void pickup_state_object_sizes(ObjectSizes& output); // Xbox 0x0009DD80
bool initialize_runtime_pickup_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimePickupStateContext& context); // Xbox 0x0009DE00
void destroy_runtime_pickup_state(std::span<u8> state, RuntimePickupStateContext& context); // Xbox 0x0009DEA0
void tick_runtime_pickup_state(std::span<u8> state, RuntimePickupStateContext& context); // Xbox 0x0009DF90
// Factory type 0x7D (getter 0x0009E450, vtable 0x00505A80): the body lifts a weapon that answers message 0xA0 (the
// trophy). +0xC is 0x0009E9F0; the +0x14 / +0x18 slots are the empty 0x001628B0 and +0x1C is 0x0002B620.
//
// State words: +0x18 the animation point that asks the weapon, +0x1C the point it reaches the hands, +0x20 the point
// after which the stick ends the state, +0x24 the hold height (times actor +0x38), +0x28 the weapon that agreed and
// +0x2C the stage (1 facing the mind's weapon, 2 asked, 3 the weapon agreed).
// The lift state's parameters (0x1C bytes) and own fields (0x30 bytes, 0x0009E430), after the body-state base.
namespace lift_state_parameter
{
constexpr RetailField<float, 0x0C> ask_point{};
constexpr RetailField<float, 0x10> hands_point{};
constexpr RetailField<float, 0x14> release_point{};
constexpr RetailField<float, 0x18> hold_height{};
constexpr u32 kSize = 0x1C;
static_assert(retail_fits(kSize, hold_height));
} // namespace lift_state_parameter
namespace lift_state_field
{
constexpr RetailField<float, 0x18> ask_point{}; // the animation point at which the weapon is asked (message 0xA0)
constexpr RetailField<float, 0x1C> hands_point{}; // the point the weapon reaches the hands
constexpr RetailField<float, 0x20> release_point{}; // the point after which the stick ends the state
constexpr RetailField<float, 0x24> hold_height{}; // times actor +0x38 in the ask
constexpr RetailField<u32, 0x28> weapon{}; // the weapon that agreed; message 0x7C naming it clears it
constexpr RetailField<u32, 0x2C> stage{}; // 1 facing the mind's weapon, 2 asked, 3 the weapon agreed
constexpr u32 kSize = 0x30;
static_assert(retail_fits(kSize, stage));
} // namespace lift_state_field
void lift_state_object_sizes(ObjectSizes& output); // Xbox 0x0009E430
bool initialize_runtime_lift_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimePickupStateContext& context); // Xbox 0x0009E4B0
void destroy_runtime_lift_state(std::span<u8> state, RuntimePickupStateContext& context); // Xbox 0x0009E560
// Turns the body toward the weapon's nearest pick-up point.
void face_runtime_lift_weapon(u32 weapon, RuntimePickupStateContext& context); // Xbox 0x0009EA70
void tick_runtime_lift_state(std::span<u8> state, RuntimePickupStateContext& context); // Xbox 0x0009E6B0
void message_runtime_lift_state(std::span<u8> state, std::span<const u32> message,
    RuntimePickupStateContext& context); // Xbox 0x0009E9F0
// Moves the body toward the weapon's nearest pick-up point while the animation runs up to `grab`.
void approach_runtime_pickup(std::span<const u8> state, RuntimePlayerAttachmentActor& weapon, float grab,
    RuntimePickupStateContext& context); // Xbox 0x0009E2B0
}
