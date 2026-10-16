#pragma once

#include "body_state_actions.h"
#include "body_attachment_cleanup.h"
#include "body_death_effects.h"
#include "actor_attachment_events.h"
#include "actor_clock.h"
#include "player_attachment.h"
#include "../scripts/script_table_lookup.h"
#include "body_message_helpers.h"
#include "gbtg/actor_runtime.h"

namespace gbtg
{
struct Game;

// Factory type 0x7A (getter 0x0009C570, vtable 0x00505A20; named entityStateLookForWeapon in the type table): the
// body's knockdown, entered from the death row. State +8 is the body. +0x14/+0x18 are the empty 0x001628B0 and
// +0x1C is 0x0002B620.
//
// State words: +0x10 the state clock, +0x18 the phase (1 flying, 2 bounce-landing, 3 landed, 4 bounce, 5 bounce
// recovery, 6 down, 7 getting up, 8 the timed end), +0x1C the phase clock, +0x20 the end timer, +0x28 the first-step
// callout sent, +0x2C body +0x715 raised, +0x30 the saved body +0x717, +0x34 parameter +0x30, +0x38..+0x50 the seven
// animation records, +0x54..+0x5A the follow-up keys, +0x5C the death key, +0x5E the next key, +0x60 / +0x64 the
// sweep's previous point, +0x70 the callout tokens (0x13B8B0), +0x94 the start point, +0xA0 the saved body +0x324,
// +0xA4 the saved shape mode, +0xA8 yaw, +0xAC pitch, +0xB0 the flight time, +0xB4..+0xBC animation marks, +0xC0
// the landing height, +0xC4 the bounce scale, +0xC8 body +0x270, +0xCC the contact pool, +0xD0 the landing point,
// +0xE0 the getting-up count released, +0xE4 the counted flag, +0xE8 the end time, +0xEC a body bit, +0xF0 the
// first-step callout id.
struct RuntimeKnockdownContext
{
    std::span<u8> body;
    Actor& actor; // Body +8.
    std::span<const u8> actor_bytes;
    u32 actor_address;
    RuntimeBodyBitStorage& bits; // Body +0x15C.
    ActorAnimationState& animation; // Actor +0x50.
    const RuntimeActorClock& clock;
    const i32& motion_steps; // DAT_004312E0.
    const Vec3& default_position; // DAT_0050DD80.
    RuntimeBodyStateContext& shape; // 0x0001FEB0's views.
    RuntimeBodyActionSelection& actions; // 0x0001AFA0, 0x0001AED0.
    RuntimePlayerAttachment& attachment; // 0x0001B810.
    ActorAttachmentEventContext& events; // 0x0013B7C0's record callback views.
    const u8* callout_records; // Actor +0x8C.
    entity_detail::ListHead& live_attachments; // Actor +0x8F4; its next (actor +0x8F8) is the first record.
    std::function<u32(const void*)> address_of;
    RuntimeBodyAttachmentLists& lists; // 0x0013B810, 0x0003CFD0.
    RuntimeBodyDeathEffects& death; // 0x00024890, 0x00134AE0.
    Game& game;
    u32& scene_flags; // DAT_0050EA9C.
    u32& restart_requested; // DAT_00430D0C.
    std::span<const u8> settings; // Body +0x268.
    const RuntimeScriptLookupTable* table; // Body +0x73C.
    u32& random; // DAT_0054DD7C.
    const u8* tag_rows; // DAT_00545AC4.
    std::span<const u8, 2> hit_bands; // DAT_00433D08 / DAT_00433D09.
    std::span<const float, 2> hit_damage; // DAT_00433D0C / DAT_00433D10.
    // The scene's play bounds (DAT_0050EA01 and DAT_0050EA7C..DAT_0050EA90).
    const u8& bounded;
    std::array<const float*, 6> bounds;
    // An XBE string the parameters name, or null for address 0.
    std::function<const char*(u32 address)> string_at;
    // 0x0003BE20 over actor +0x8C with callback 0 and value 0.
    std::function<void(u32 id, const RuntimeActorCreationOptions* options)> callouts;
    std::function<void(std::span<const u32>)> actor_message; // Actor vtable +0xC; zero-filled past the given words.
    // 0x0013FC50 over the DAT_00511A1C slots the last probe filled: message 8 to each, the replies' +4 summed.
    std::function<i32(i32 count, u32 player, u32 actor)> probe_messages;
    std::function<std::span<const u8>(u32 address, size_t size)> record_at; // A scene record's bytes.
};

// Message 8 from the knockdown sweep (0x0013FC50) to a probe's owner: +4 the owner's reply, +8 its contact
// records, +0xC the sweeping player flag, +0x10 the sweeping actor.
struct RuntimeSweepNotice
{
    u32 id;
    u32 reply;
    GhoulySpawnPool* contacts;
    u32 player;
    u32 actor;
};

void knockdown_state_object_sizes(ObjectSizes& output); // Xbox 0x0009C550
bool initialize_runtime_knockdown_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimeKnockdownContext& context); // Xbox 0x0009C5D0
void destroy_runtime_knockdown_state(std::span<u8> state, RuntimeKnockdownContext& context); // Xbox 0x0009CA80
void message_runtime_knockdown_state(std::span<u8> state, std::span<const u8> message,
    RuntimeKnockdownContext& context); // Xbox 0x0009D0E0
// 0x0009D0E0 for a collision reply (body vtable +0x1C), whose +0x1C contact record is a host pointer.
void message_runtime_knockdown_state(std::span<u8> state, RuntimePlayerCollisionReply& reply,
    RuntimeKnockdownContext& context); // Xbox 0x0009D0E0
// Message 0x62 (EAX state, ESI the reply): accepts while flying and marks a wall the actor +0x3C motion runs into.
void collide_runtime_knockdown(std::span<const u8> state, RuntimePlayerCollisionReply& reply,
    RuntimeKnockdownContext& context); // Xbox 0x0009B720
void tick_runtime_knockdown_state(std::span<u8> state, RuntimeKnockdownContext& context); // Xbox 0x0009CBE0

void aim_runtime_knockdown(std::span<u8> state, RuntimeKnockdownContext& context); // Xbox 0x0009C150
void launch_runtime_knockdown(std::span<u8> state, float distance, float height, float time, float pitch,
    RuntimeKnockdownContext& context); // Xbox 0x0009B350
// The launch velocity (ESI) for `angle` at a random spread of `spread` (XMM0), with the gravity and flight time.
void runtime_knockdown_launch(float angle, float height, float distance, float jitter, float time, float spread,
    Vec3& velocity, float& gravity, float& flight, u32& random); // Xbox 0x00140BF0
void enter_runtime_knockdown_phase(std::span<u8> state, u32 phase,
    RuntimeKnockdownContext& context); // Xbox 0x0009D150
void sweep_runtime_knockdown(std::span<u8> state, RuntimeKnockdownContext& context); // Xbox 0x0009BEE0
u32 runtime_knockdown_contact_kind(std::span<const u8> state, RuntimeKnockdownContext& context); // Xbox 0x0009C100
void fly_runtime_knockdown(std::span<u8> state, float timer, RuntimeKnockdownContext& context); // Xbox 0x0009B430
bool runtime_knockdown_hits_wall(std::span<const u8> record, const Vec3& velocity); // Xbox 0x0009BA40, ECX record
void bounce_runtime_knockdown(std::span<u8> state, std::span<const u8> record,
    RuntimeKnockdownContext& context); // Xbox 0x0009B780, EAX record
void end_runtime_knockdown(std::span<u8> state, u16 key, RuntimeKnockdownContext& context); // Xbox 0x0009D5C0
void finish_runtime_knockdown(std::span<u8> state, RuntimeKnockdownContext& context); // Xbox 0x0009D670
void slow_runtime_knockdown(RuntimeKnockdownContext& context); // Xbox 0x0009D6D0
void settle_runtime_knockdown(std::span<u8> state, RuntimeKnockdownContext& context); // Xbox 0x0009D7D0
void clear_runtime_body_contacts(RuntimeKnockdownContext& context); // Xbox 0x00025FA0
void signal_runtime_knockdown(const Vec3& position, float first, float second,
    RuntimeKnockdownContext& context); // Xbox 0x0009D810
float runtime_floor_pitch(const Actor& actor, std::span<const u8> actor_bytes); // Xbox 0x00136150, EAX actor
void pose_runtime_knockdown(std::span<u8> state, float timer, RuntimeKnockdownContext& context); // Xbox 0x0009C220
void notify_runtime_knockdown_contacts(std::span<u8> state, RuntimeKnockdownContext& context); // Xbox 0x0009BD40
}
