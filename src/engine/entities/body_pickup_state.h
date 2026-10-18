#pragma once

#include "body_state_actions.h"
#include "player_attachment.h"
#include "actor_clock.h"
#include "actor_live_attachments.h"
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
};

void pickup_state_object_sizes(ObjectSizes& output); // Xbox 0x0009DD80
bool initialize_runtime_pickup_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimePickupStateContext& context); // Xbox 0x0009DE00
void destroy_runtime_pickup_state(std::span<u8> state, RuntimePickupStateContext& context); // Xbox 0x0009DEA0
void tick_runtime_pickup_state(std::span<u8> state, RuntimePickupStateContext& context); // Xbox 0x0009DF90
// Moves the body toward the weapon's nearest pick-up point while the animation runs up to `grab`.
void approach_runtime_pickup(std::span<const u8> state, RuntimePlayerAttachmentActor& weapon, float grab,
    RuntimePickupStateContext& context); // Xbox 0x0009E2B0
}
