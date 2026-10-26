#pragma once

#include "body_action_selection.h"
#include "body_animation_setup.h"
#include "body_damage_tables.h"
#include "body_resources.h"
#include "body_timers.h"
#include "actor_clock.h"
#include "actor_render_registration.h"
#include "actor_trail_requests.h"
#include "../frontend/dialogue_input.h"
#include "../scripts/script_table_lookup.h"

namespace gbtg
{
struct RuntimePlayerGoalRing;

// Factory type 0x73 entityStateHit (getter 0x00095740, vtable 0x00505940): +0x14/+0x18 are the empty 0x001628B0
// and +0x1C is 0x0002B620. State +8 is the body.
struct RuntimeHitStateLifecycle
{
    std::span<u8> body;
    Actor& actor; // Body +8.
    std::span<u8> actor_bytes;
    RuntimeBodyBitStorage*& bits; // Body +0x15C.
    std::span<const u8> target; // Actor +0x958 -> +0x14: the mind's target record, or empty.
};
// The angle `current` moved by `fraction` of its signed turn toward `target` (XMM0), in [0, 2pi).
float step_runtime_angle_toward(float target, float current, float fraction); // Xbox 0x001013A0
void hit_state_object_sizes(ObjectSizes& output); // Xbox 0x00095720
bool initialize_runtime_hit_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimeHitStateLifecycle& context); // Xbox 0x000957A0
void destroy_runtime_hit_state(std::span<u8> state, RuntimeHitStateLifecycle& context); // Xbox 0x000958A0

// Message views of the hit state's body (state +8).
struct RuntimeHitStateMessages
{
    std::span<u8> body;
    u32 actor_address; // Body +8.
    std::span<const u8> target; // Actor +0x958 -> +0x14: the mind's target record, or empty.
    std::span<const u8> event; // Message 7 +0xC: the animation event's bytes.
    const Vec3* contact; // Ids 0x41 and 0x5A: the reported contact point (EAX of 0x000961C0), or null.
    u32 body_attached; // Actor +0x95C -> +0x48 (id 0x5A).
    // 0x0003BE20 over actor `actor`'s +0x8C callout records with g_actor_creation_options and callback 0.
    std::function<void(u32 actor, u32 id)> callouts;
};
void message_runtime_hit_state(std::span<u8> state, std::span<const u8> message,
    RuntimeHitStateMessages& context); // Xbox 0x00095C90

// Native views of the hit state's body (state +8) and actor (body +8).
struct RuntimeHitStateTick
{
    std::span<u8> body;
    Actor& actor;
    std::span<u8> actor_bytes;
    RuntimeBodyBitStorage*& bits; // Body +0x15C.
    const ActorAnimationState& animation; // Actor +0x50.
    const RuntimeActorClock& clock;
    const i32& motion_steps; // DAT_004312E0.
    const Vec3& default_position; // DAT_0050DD80.
    std::span<const u8> target; // Actor +0x958 -> +0x14: the mind's target record, or empty.
    std::span<const u8> hit; // Body +0x84.
    std::span<const u8> settings; // Body +0x268.
    const RuntimeBodyCollisionState* collision; // Body +0x2C8.
    const RuntimeBodyCollisionState* attached_collision; // Body +0x48 -> +0x998.
    const ActorRenderRegistration& render; // Actor +0x124 bins.
    ActorTrailContext& trails;
    u32 input_word; // The tick's second argument.
    const RuntimeDialogueInputDevice* input; // The tick's input word, or null.
    const RuntimeDialogueInputDevice* own_input; // Actor +0x960, or null.
    const RuntimeDialogueInputState& input_state;
    std::span<const u8> address_zero; // What a null input word reads.
    const RuntimeBodyDamageTables& tables;
    std::span<const float> mode_scales; // DAT_00217EC0.
    const RuntimeScriptLookupTable* table; // Body +0x73C.
    RuntimeBodyTimerRecipient* first_timer; // Body +0x760.
    std::function<void(std::span<const u32>)> actor_message; // Actor vtable +0xC; the rest of the stack follows.
    std::function<u16()> primary_key; // Body vtable +0x20.
    std::function<u16()> secondary_key; // Body vtable +0x2C.
};
bool runtime_actor_has_render_bins(const ActorRenderRegistration& render, u8 hidden); // Xbox 0x0003CCB0
// The body views 0x00134180 reads.
struct RuntimeBodyNextAction
{
    std::span<u8> body;
    std::span<const u8> settings; // Body +0x268.
    const RuntimeScriptLookupTable* table; // Body +0x73C.
    RuntimeBodyTimerRecipient* first_timer; // Body +0x760.
    std::function<u16()> primary_key; // Body vtable +0x20.
    std::function<u16()> secondary_key; // Body vtable +0x2C.
};
// Picks the body's next action into state +0xC (EAX body, EBX state).
void select_runtime_body_next_action(std::span<u8> state, u32 secondary,
    RuntimeBodyNextAction& context); // Xbox 0x00134180
RuntimeBodyNextAction runtime_body_next_action(const RuntimeHitStateTick& context);
void reset_runtime_body_turn(u32 brake, std::span<u8> body,
    RuntimeBodyTimerRecipient* first_timer); // Xbox 0x001346B0, ESI body
float runtime_hit_target_gap(float& angle, RuntimeHitStateTick& context); // Xbox 0x00096260
void lunge_runtime_hit_state(std::span<u8> state, RuntimeHitStateTick& context); // Xbox 0x00095F30
// The turn toward `target` from `current` that the animation reaches by the hit record's window (body +0x84 +0x18),
// or body +0x730 once the window has passed (EAX target, then the body, the current euler and the output).
bool turn_runtime_body_toward(const Vec3& target, std::span<const u8> body, const Vec3& current, Vec3& euler,
    std::span<const u8> hit, const Actor& actor, const ActorAnimationState& animation); // Xbox 0x0001D5B0
void face_runtime_hit_target(std::span<const u8> state, RuntimeHitStateTick& context); // Xbox 0x00095EA0
void steer_runtime_hit_body(RuntimeHitStateTick& context); // Xbox 0x000963E0
void move_runtime_hit_body(std::span<const u8> state, RuntimeHitStateTick& context); // Xbox 0x00096060
void finish_runtime_hit_state(std::span<u8> state, u32 secondary, RuntimeHitStateTick& context); // Xbox 0x00096390
void tick_runtime_hit_state(std::span<u8> state, RuntimeHitStateTick& context); // Xbox 0x00095940

// Factory type 0x74 entityStateHitFurnace (getter 0x000966B0, vtable 0x00505960): the message and the +0x14 / +0x18
// slots are the empty 0x001628B0 and +0x1C is 0x0002B620. State +8 is the body.
void hit_furnace_state_object_sizes(ObjectSizes& output); // Xbox 0x00090410
bool initialize_runtime_hit_furnace_state(std::span<u8> state, std::span<const u8> parameters, std::span<u8> body,
    RuntimeBodyBitStorage& bits); // Xbox 0x00096710
void destroy_runtime_hit_furnace_state(std::span<const u8> state, std::span<u8> body,
    RuntimeBodyBitStorage& bits); // Xbox 0x000967F0

// The body's next action after a recovery (EAX body).
struct RuntimeBodyRecoveryAction
{
    std::span<u8> body;
    std::span<const u8> settings; // Body +0x268.
    const RuntimeScriptLookupTable* table; // Body +0x73C.
    RuntimeBodyTimerRecipient* first_timer; // Body +0x760.
    const RuntimePlayerGoalRing* ring; // Actor +0x980.
    u16 goal_latch; // Actor +0x990.
    std::span<const u8> address_zero; // What an empty goal ring's front row reads.
    // 0x00028360 then 0x00135790 (actor +0x990 zero) and 0x00135540 (otherwise) over the front goal: each fills
    // the output word the caller tests.
    std::function<void(u32& output)> kind_goal_action;
    std::function<void(u32& output)> player_goal_action;
    std::function<u16()> primary_key; // Body vtable +0x20.
    std::function<u16()> primary_id; // Body vtable +0x30.
    std::function<u16()> secondary_id; // Body vtable +0x34.
};
void select_runtime_body_recovery_action(std::span<u8> state, u16 key,
    RuntimeBodyRecoveryAction& context); // Xbox 0x00133F50
void select_runtime_body_goal_action(std::span<u8> state,
    RuntimeBodyRecoveryAction& context); // Xbox 0x00134060, EBX body

struct RuntimeHitFurnaceTick
{
    std::span<u8> body;
    Actor& actor; // Body +8.
    std::span<const u8> actor_bytes;
    const ActorAnimationState& animation; // Actor +0x50.
    const RuntimeActorClock& clock;
    std::span<const u8> settings; // Body +0x268.
    const RuntimeDialogueInputDevice* input; // The tick's input word, or null.
    const RuntimeDialogueInputState& input_state;
    std::span<const u8> address_zero; // What a null input word reads.
    u32 actor_address; // Body +8 as a retail word, for 0x0013FC50.
    // 0x0013FC50: each collected probe hears message 8 with the attacker-is-a-player flag and the actor.
    std::function<i32(i32 count, u32 player, u32 actor)> probe_messages;
    std::function<std::span<const u8>(u32 address, size_t size)> record_at; // A scene record's bytes.
    RuntimeBodyRecoveryAction& recovery;
};
// The burning body's contacts (mask 0x140) within its sweep radius, and along its path from the last point
// (state +0x24, kept while +0x20 is set), hear the scorch through 0x0013FC50.
void scorch_runtime_hit_furnace_contacts(std::span<u8> state, RuntimeHitFurnaceTick& context); // Xbox 0x000965C0
void tick_runtime_hit_furnace_state(std::span<u8> state, RuntimeHitFurnaceTick& context); // Xbox 0x00096890
}
