#pragma once
#include "body_contacts.h"
#include "body_message_helpers.h"
#include "player_goal_cleanup.h"
#include "../graphics/actor_effect_stream.h"

namespace gbtg
{
float runtime_actor_animation_duration(const ActorAnimationState& animation,
    u32 slot); // Xbox 0x0003B1D0
void command_runtime_body_breath(Actor& actor, const EntityBitBlock& flags,
    const ActorAnimationState& animation, u32 phase, u32 slot,
    std::span<const u8> command, u32 effect_flags,
    ActorEffectStreamContext& effects); // Xbox 0x0001AC50
struct RuntimeActorProximityNode
{
    RuntimeActorProximityNode* next; // Actor intrusive link +0x938.
    Actor& actor;
    std::function<void(std::span<const u32>)> message; // Actor vtable +0xC.
};
void broadcast_runtime_actor_proximity(RuntimeActorProximityNode* first,
    std::span<const u32> message, const Vec3& position, float radius); // Xbox 0x00122D80
void command_runtime_body_proximity(Actor& actor, std::span<const u8> command,
    RuntimeActorProximityNode* first); // Xbox 0x0001ADB0
u32 teleport_runtime_body_secondary(std::span<u8> body, RuntimeBodyAttachedActor* primary,
    RuntimeBodyAttachedActor*& secondary, RuntimeBodyAnimationReceiver*& receiver,
    const Vec3& position); // Xbox 0x0001B5D0
void command_runtime_body_teleport(std::span<u8> body, Actor& actor,
    std::span<const u8> command, RuntimeBodyAttachedActor* primary,
    RuntimeBodyAttachedActor*& secondary, RuntimeBodyAnimationReceiver*& receiver); // Xbox 0x0001A750
struct RuntimeBodyCommandCallout
{
    u32 actor_address;
    const u8* record;
    ActorAttachmentEventContext& events;
};
void command_runtime_body_attachment_event(RuntimeBodyCommandCallout* attached); // Xbox 0x0001A720
void command_runtime_body_primary_callout(RuntimeBodyCommandCallout* attached,
    const char* name, std::span<const char* const, 0x55> names); // Xbox 0x0001A810
void command_runtime_body_secondary_callout(RuntimeBodyCommandCallout* attached,
    const char* name, std::span<const char* const, 0x55> names); // Xbox 0x0001A880
void command_runtime_body_detach_actors(std::span<u8> body,
    std::span<const u8> command, RuntimeBodyActorAttachment& primary,
    RuntimeBodyAttachedActor*& secondary, RuntimeBodyAnimationReceiver*& receiver); // Xbox 0x0001A690
void command_runtime_body_facing_push(Actor& actor, const EntityBitBlock& flags,
    std::span<const u8> command); // Xbox 0x00019A50
float runtime_actor_relative_target_angle(const Actor& actor,
    const Vec3& target_position); // Xbox 0x00041A30
struct RuntimeBodyCommandTarget
{
    const Vec3* position; // Mind +0x14 object's stored position at +0x20.
};
void command_runtime_body_target_push(Actor& actor, const EntityBitBlock& flags,
    const RuntimeBodyCommandTarget* mind, std::span<const u8> command); // Xbox 0x000199F0
void record_runtime_body_attachment_event(const u8* record,
    const RuntimeActorCreationOptions* options, ActorAttachmentEventContext& context,
    u32 value); // Xbox 0x0013B8B0
void request_runtime_player_attachment_events(const u8* record,
    ActorAttachmentEventContext& context, i32 id, u32 output_address); // Xbox 0x0013B7E0
void clear_runtime_player_attachment_events(std::span<u8> events,
    RuntimeBodyAttachmentLists& lists); // Xbox 0x0013B810
void command_runtime_body_attachment_list(std::span<u8> body, u32 body_address,
    bool clear, const u8* record, ActorAttachmentEventContext& events,
    RuntimeBodyAttachmentLists& lists); // Xbox 0x0001AE00
u32 runtime_body_flag_name_index(const char* name,
    std::span<const char* const, 0x41> names); // Xbox 0x0001EF70
struct RuntimeBodyCommandAttachment
{
    RuntimeBodyAttachedActor& actor;
    RuntimeBodyCollisionState*& collision; // Attached actor +0x998.
};
void activate_runtime_attached_contact_shapes(RuntimeBodyCommandAttachment& attached,
    u16 tag, u8 flags, float scale); // Xbox 0x00144E90
void deactivate_runtime_attached_contact_shapes(RuntimeBodyCommandAttachment& attached,
    u16 tag); // Xbox 0x00144EE0
void command_runtime_attached_contact_shapes(RuntimeBodyCommandAttachment* attached,
    std::span<const u8> command, u32 phase); // Xbox 0x0001A290
void command_runtime_body_contact_shapes(std::span<u8> body, Actor& actor,
    RuntimeBodyCollisionState* state, std::span<const u8> command,
    u32 phase); // Xbox 0x0001AAF0
void command_runtime_body_side_counters(std::span<u8> body, u32 mask,
    u32 phase); // Xbox 0x0001AB50
void command_runtime_body_latch_mask(std::span<u8> body, Actor& actor,
    entity_detail::LatchNode*& first, u8 mask, u32 phase); // Xbox 0x0001ABC0
void command_runtime_body_flag(EntityBitBlock& flags, const char* name,
    std::span<const char* const, 0x41> names, u32 phase); // Xbox 0x0001ABF0
void command_runtime_body_release_actor(std::span<u8> body,
    RuntimeBodyActorAttachment& attachment); // Xbox 0x0001A7E0
void command_runtime_body_random_timer(std::span<u8> body, float low,
    float high, u32& random); // Xbox 0x0001AE50
float runtime_body_random_range(u32& random, float low, float high); // Xbox 0x000191C0
void add_runtime_body_direction_push(Actor& actor, i32 speed,
    float angle); // Xbox 0x00019A90
}
