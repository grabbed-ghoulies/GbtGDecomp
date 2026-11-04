#pragma once

#include "body_collision.h"
#include "body_state_actions.h"
#include "body_attachment_cleanup.h"
#include "body_death_effects.h"
#include "actor_attachment_events.h"
#include "actor_clock.h"
#include "player_attachment.h"
#include "../scripts/script_table_lookup.h"
#include "body_message_helpers.h"
#include "body_state_fields.h"
#include "gbtg/actor_runtime.h"
#include "gbtg/retail_field.h"

#include <array>

namespace gbtg
{
struct Game;

// Factory type 0x7A (getter 0x0009C570, vtable 0x00505A20; named entityStateLookForWeapon in the type table): the
// body's knockdown, entered from the death row. State +8 is the body, +0xC the next key and +0x10 the state clock
// (`body_state_field`). +0x14/+0x18 are the empty 0x001628B0 and +0x1C is 0x0002B620. The phases (+0x18): 1 flying,
// 2 bounce-landing, 3 landed, 4 bounce, 5 bounce recovery, 6 down, 7 getting up, 8 the timed end.

// The seven animations the parameters name (+0xC..+0x24) and the state's found flags (+0x38..+0x50) index.
enum KnockdownAnimation : u32
{
    kKnockdownFlight = 0,
    kKnockdownBounceLanding = 1,
    kKnockdownLanded = 2,
    kKnockdownLandedVariant = 3, // played instead of kKnockdownLanded when it was found
    kKnockdownBounce = 4,
    kKnockdownBounceRecovery = 5,
    kKnockdownGettingUp = 6,
    kKnockdownAnimationCount = 7,
};
namespace knockdown_state_parameter
{
// The seven animations' names, XBE strings, in KnockdownAnimation order.
constexpr RetailField<std::array<u32, kKnockdownAnimationCount>, 0x0C> animations{};
constexpr RetailField<u16, 0x28> getting_up_key{};
constexpr RetailField<u16, 0x2A> landing_key{};
constexpr RetailField<u16, 0x2C> bounce_key{};
constexpr RetailField<u16, 0x2E> fallback_death_key{};
constexpr RetailField<u32, 0x30> recovery_mode{};
constexpr RetailField<float, 0x34> pitch{}; // degrees
constexpr RetailField<float, 0x38> end_time{};
constexpr RetailField<float, 0x3C> rise_height{}; // of the actor's +0x34 height
constexpr RetailField<float, 0x40> bounce_scale{};
// 0 no latches, 1 the state holds the body's latches, 2 only when the body has no energy left.
constexpr RetailField<u32, 0x44> latch_mode{};
constexpr RetailField<u32, 0x48> body_bit{};
constexpr RetailField<u32, 0x4C> first_callout{};
constexpr u32 kSize = 0x50;
static_assert(retail_fits(kSize, first_callout));
} // namespace knockdown_state_parameter
// The callout tokens 0x0013B7C0 records for callouts 0x4A; the attachment functions take its retail_bytes.
struct KnockdownCalloutTokens
{
    u32 count = 0; // +0
    std::array<u32, 8> ids{}; // +4
};
static_assert(sizeof(KnockdownCalloutTokens) == 0x24);
struct KnockdownState
{
    BodyStateBase base{}; // +0
    u32 phase = 0; // +0x18
    float phase_time = 0.f; // +0x1C
    float end_timer = 0.f; // +0x20, phase 8 ends when it runs out
    std::array<u8, 4> unk_24{}; // +0x24, never read or written by translated code
    u32 first_callout_sent = 0; // +0x28
    u32 contacts_raised = 0; // +0x2C, body +0x715 is raised; cleared with the body's contacts
    u32 saved_717 = 0; // +0x30, body +0x717 at entry, restored by the destructor
    // +0x34: Parameter +0x30. 1: a living body ends the knockdown on landing (and late in the flight) instead of going
    // down.
    u32 recovery_mode = 0;
    // +0x38: Whether each animation's record was found, in KnockdownAnimation order.
    std::array<u32, kKnockdownAnimationCount> animations_found{};
    // +0x54: The keys the knockdown ends with: after getting up, after landing, after a bounce, and when the body has
    // no energy left (the death key, else the fallback death key).
    u16 getting_up_key = 0;
    u16 landing_key = 0; // +0x56
    u16 fallback_death_key = 0; // +0x58
    u16 bounce_key = 0; // +0x5A, nonzero also lets a late wall hit bounce at random
    u16 death_key = 0; // +0x5C, action 0x68 or 0x69, when the body's table has it
    u16 end_key = 0; // +0x5E, the key end_runtime_knockdown chose; finish hands it on
    u32 swept = 0; // +0x60, sweep_point holds the previous sweep's position
    Vec3 sweep_point{}; // +0x64
    KnockdownCalloutTokens callouts{}; // +0x70
    Vec3 start_point{}; // +0x94
    float saved_gravity = 0.f; // +0xA0, body +0x324 at entry, restored when the body is down
    u8 saved_shape_mode = 0; // +0xA4
    std::array<u8, 3> unk_A5{}; // +0xA5, padding
    float yaw = 0.f; // +0xA8
    float pitch = 0.f; // +0xAC, the launch pitch the flight eases back to zero
    float flight_time = 0.f; // +0xB0
    // +0xB4: The animation position and the actor's pitch and yaw when the landing began; the pose eases from them.
    float pose_mark = 0.f;
    float start_pitch = 0.f; // +0xB8
    float start_yaw = 0.f; // +0xBC
    float rise_height = 0.f; // +0xC0, a down body nearer its floor than this gets up
    float bounce_scale = 0.f; // +0xC4
    u32 attacker = 0; // +0xC8, body +0x270 at entry; its pushes aren't recorded
    u32 contact_pool = 0; // +0xCC, nonzero while the host contact pool exists
    Vec3 landing_point{}; // +0xD0
    std::array<u8, 4> unk_DC{}; // +0xDC, never read or written by translated code
    u32 getting_up_released = 0; // +0xE0, body +0x69 was dropped
    u32 holds_latches = 0; // +0xE4, the latches are held; the destructor drops body +0x7E5
    float end_time = 0.f; // +0xE8, parameter +0x38: phase 8's time for a body with no energy left
    u32 body_bit = 0; // +0xEC, a body bit raised while the state lasts
    u32 first_callout = 0; // +0xF0, the callout sent on the first step, else 0x3B
};
static_assert(sizeof(KnockdownState) == 0xF4);
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
    std::function<Vec3(u32 actor)> actor_position; // 0x000412A0 on another scene actor.
    // Another scene actor's vtable +0xC; its replies land in the words.
    std::function<void(u32 actor, std::span<u32> message)> send;
    std::function<void()> hold_latches; // 0x00027BE0 on this body.
    RuntimeBodyCollisionSphere* spheres; // Body +0x710 (0x00026010's records); the count is body +0x714.
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
bool initialize_runtime_knockdown_state(KnockdownState& state, std::span<const u8> parameters,
    RuntimeKnockdownContext& context); // Xbox 0x0009C5D0
void destroy_runtime_knockdown_state(KnockdownState& state, RuntimeKnockdownContext& context); // Xbox 0x0009CA80
void message_runtime_knockdown_state(KnockdownState& state, std::span<const u8> message,
    RuntimeKnockdownContext& context); // Xbox 0x0009D0E0
void record_runtime_knockdown_contact(KnockdownState& state, u32 other, const Vec3& contact, u8 flag, u16 bone,
    RuntimeKnockdownContext& context); // Xbox 0x0009BB60, EAX other
void forget_runtime_knockdown_contact(KnockdownState& state, u32 other); // Xbox 0x0009BEA0, EAX state, EDX other
// 0x0009D0E0 for a collision reply (body vtable +0x1C), whose +0x1C contact record is a host pointer.
void message_runtime_knockdown_state(KnockdownState& state, RuntimePlayerCollisionReply& reply,
    RuntimeKnockdownContext& context); // Xbox 0x0009D0E0
// Message 0x62 (EAX state, ESI the reply): accepts while flying and marks a wall the actor +0x3C motion runs into.
void collide_runtime_knockdown(const KnockdownState& state, RuntimePlayerCollisionReply& reply,
    RuntimeKnockdownContext& context); // Xbox 0x0009B720
void tick_runtime_knockdown_state(KnockdownState& state, RuntimeKnockdownContext& context); // Xbox 0x0009CBE0

void aim_runtime_knockdown(KnockdownState& state, RuntimeKnockdownContext& context); // Xbox 0x0009C150
void launch_runtime_knockdown(KnockdownState& state, float distance, float height, float time, float pitch,
    RuntimeKnockdownContext& context); // Xbox 0x0009B350
// The launch velocity (ESI) for `angle` at a random spread of `spread` (XMM0), with the gravity and flight time.
void runtime_knockdown_launch(float angle, float height, float distance, float jitter, float time, float spread,
    Vec3& velocity, float& gravity, float& flight, u32& random); // Xbox 0x00140BF0
void enter_runtime_knockdown_phase(KnockdownState& state, u32 phase,
    RuntimeKnockdownContext& context); // Xbox 0x0009D150
void sweep_runtime_knockdown(KnockdownState& state, RuntimeKnockdownContext& context); // Xbox 0x0009BEE0
u32 runtime_knockdown_contact_kind(const KnockdownState& state, RuntimeKnockdownContext& context); // Xbox 0x0009C100
void fly_runtime_knockdown(KnockdownState& state, float timer, RuntimeKnockdownContext& context); // Xbox 0x0009B430
bool runtime_knockdown_hits_wall(std::span<const u8> record, const Vec3& velocity); // Xbox 0x0009BA40, ECX record
void bounce_runtime_knockdown(KnockdownState& state, std::span<const u8> record,
    RuntimeKnockdownContext& context); // Xbox 0x0009B780, EAX record
void end_runtime_knockdown(KnockdownState& state, u16 key, RuntimeKnockdownContext& context); // Xbox 0x0009D5C0
void finish_runtime_knockdown(KnockdownState& state, RuntimeKnockdownContext& context); // Xbox 0x0009D670
void slow_runtime_knockdown(RuntimeKnockdownContext& context); // Xbox 0x0009D6D0
void settle_runtime_knockdown(KnockdownState& state, RuntimeKnockdownContext& context); // Xbox 0x0009D7D0
void clear_runtime_body_contacts(RuntimeKnockdownContext& context); // Xbox 0x00025FA0
void signal_runtime_knockdown(const Vec3& position, float first, float second,
    RuntimeKnockdownContext& context); // Xbox 0x0009D810
float runtime_floor_pitch(const Actor& actor, std::span<const u8> actor_bytes); // Xbox 0x00136150, EAX actor
void pose_runtime_knockdown(KnockdownState& state, float timer, RuntimeKnockdownContext& context); // Xbox 0x0009C220
void notify_runtime_knockdown_contacts(KnockdownState& state, RuntimeKnockdownContext& context); // Xbox 0x0009BD40
}
