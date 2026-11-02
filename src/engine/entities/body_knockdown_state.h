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
namespace knockdown_state_field
{
constexpr RetailField<u32, 0x18> phase{};
constexpr RetailField<float, 0x1C> phase_time{};
constexpr RetailField<float, 0x20> end_timer{}; // phase 8 ends when it runs out
constexpr RetailField<u32, 0x28> first_callout_sent{};
constexpr RetailField<u32, 0x2C> contacts_raised{}; // body +0x715 is raised; cleared with the body's contacts
constexpr RetailField<u32, 0x30> saved_717{}; // body +0x717 at entry, restored by the destructor
// Parameter +0x30. 1: a living body ends the knockdown on landing (and late in the flight) instead of going down.
constexpr RetailField<u32, 0x34> recovery_mode{};
// Whether each animation's record was found, in KnockdownAnimation order.
constexpr RetailField<std::array<u32, kKnockdownAnimationCount>, 0x38> animations_found{};
// The keys the knockdown ends with: after getting up, after landing, after a bounce, and when the body has no energy
// left (the death key, else the fallback death key).
constexpr RetailField<u16, 0x54> getting_up_key{};
constexpr RetailField<u16, 0x56> landing_key{};
constexpr RetailField<u16, 0x58> fallback_death_key{};
constexpr RetailField<u16, 0x5A> bounce_key{}; // nonzero also lets a late wall hit bounce at random
constexpr RetailField<u16, 0x5C> death_key{}; // action 0x68 or 0x69, when the body's table has it
constexpr RetailField<u16, 0x5E> end_key{}; // the key end_runtime_knockdown chose; finish hands it on
constexpr RetailField<u32, 0x60> swept{}; // sweep_point holds the previous sweep's position
constexpr RetailField<Vec3, 0x64> sweep_point{};
// The callout tokens 0x0013B7C0 records for callouts 0x4A (a count and up to eight tokens, 0x24 bytes).
constexpr RetailRecord<0x70, 0x24> callout_tokens{};
constexpr RetailField<u32, 0x70> callout_count{};
constexpr RetailField<std::array<u32, 8>, 0x74> callout_ids{};
constexpr RetailField<Vec3, 0x94> start_point{};
constexpr RetailField<float, 0xA0> saved_gravity{}; // body +0x324 at entry, restored when the body is down
constexpr RetailField<u8, 0xA4> saved_shape_mode{};
constexpr RetailField<float, 0xA8> yaw{};
constexpr RetailField<float, 0xAC> pitch{}; // the launch pitch the flight eases back to zero
constexpr RetailField<float, 0xB0> flight_time{};
// The animation position and the actor's pitch and yaw when the landing began; the pose eases from them.
constexpr RetailField<float, 0xB4> pose_mark{};
constexpr RetailField<float, 0xB8> start_pitch{};
constexpr RetailField<float, 0xBC> start_yaw{};
constexpr RetailField<float, 0xC0> rise_height{}; // a down body nearer its floor than this gets up
constexpr RetailField<float, 0xC4> bounce_scale{};
constexpr RetailField<u32, 0xC8> attacker{}; // body +0x270 at entry; its pushes aren't recorded
constexpr RetailField<u32, 0xCC> contact_pool{}; // nonzero while the host contact pool exists
constexpr RetailField<Vec3, 0xD0> landing_point{};
constexpr RetailField<u32, 0xE0> getting_up_released{}; // body +0x69 was dropped
constexpr RetailField<u32, 0xE4> holds_latches{}; // the latches are held; the destructor drops body +0x7E5
constexpr RetailField<float, 0xE8> end_time{}; // parameter +0x38: phase 8's time for a body with no energy left
constexpr RetailField<u32, 0xEC> body_bit{}; // a body bit raised while the state lasts
constexpr RetailField<u32, 0xF0> first_callout{}; // the callout sent on the first step, else 0x3B
constexpr u32 kSize = 0xF4;
static_assert(retail_fits(kSize, first_callout));
} // namespace knockdown_state_field
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
bool initialize_runtime_knockdown_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimeKnockdownContext& context); // Xbox 0x0009C5D0
void destroy_runtime_knockdown_state(std::span<u8> state, RuntimeKnockdownContext& context); // Xbox 0x0009CA80
void message_runtime_knockdown_state(std::span<u8> state, std::span<const u8> message,
    RuntimeKnockdownContext& context); // Xbox 0x0009D0E0
void record_runtime_knockdown_contact(std::span<u8> state, u32 other, const Vec3& contact, u8 flag, u16 bone,
    RuntimeKnockdownContext& context); // Xbox 0x0009BB60, EAX other
void forget_runtime_knockdown_contact(std::span<u8> state, u32 other); // Xbox 0x0009BEA0, EAX state, EDX other
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
