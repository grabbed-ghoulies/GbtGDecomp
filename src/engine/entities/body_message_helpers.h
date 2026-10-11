#pragma once
#include "player_attachment.h"
#include "actor_attachment_events.h"

namespace gbtg
{
void set_runtime_body_bit(EntityBitBlock& flags, u32 index); // Xbox 0x0001EFD0
void clear_runtime_body_bit(EntityBitBlock& flags, u32 index); // Xbox 0x0001F030
void clear_runtime_body_message_receivers(std::span<u8> body, u32 address); // Xbox 0x0001AEB0
void clear_runtime_body_damage_sources(std::span<u8> body, u32 address); // Xbox 0x00024340
void clear_runtime_body_timer_recipient(std::span<u8> body, u32 address); // Xbox 0x00027B20
RuntimeBodyAttachedActor* detach_runtime_body_secondary(std::span<u8> body,
    RuntimeBodyAttachedActor*& secondary, RuntimeBodyAnimationReceiver*& receiver); // Xbox 0x0001B570
void clear_runtime_body_actor_reference(std::span<u8> body, u32 address,
    RuntimeBodyActorAttachment& primary, RuntimeBodyAttachedActor*& secondary,
    RuntimeBodyAnimationReceiver*& receiver); // Xbox 0x0001B100
bool runtime_body_latches_block(std::span<const u8> body, const Actor& actor,
    u32 mask); // Xbox 0x0001F750
bool runtime_body_can_receive_push(std::span<const u8> actor,
    std::span<const u8> source, i32 mode); // Xbox 0x0001D4C0
void receive_runtime_body_push(std::span<const u8> body, Actor& actor,
    std::span<const u8> actor_bytes, std::span<const u8> source,
    const u8* parameters, const Vec3& push); // Xbox 0x00024BA0
struct RuntimeBodyActionMessages
{
    RuntimePlayerAttachment& attachment;
    RuntimeBodyActionSelection& actions;
    std::function<void(std::span<const u32>)> actor_message; // Actor vtable +0xC.
    const u8*& event_record; // Actual actor +0x8C.
    ActorAttachmentEventContext& events;
};
bool notify_runtime_body_action_key(std::span<u8> body, u16 key,
    RuntimeBodyActionMessages& context); // Xbox 0x00024B20
void clear_runtime_body_action_children(std::span<u8> body,
    RuntimeBodyActionSelection& context); // Xbox 0x000275E0
void reselect_runtime_body_action(std::span<u8> body,
    RuntimeBodyActionMessages& context); // Xbox 0x000277E0
void reset_runtime_body_action(std::span<u8> body, std::span<const u8> settings,
    RuntimeBodyActionMessages& context); // Xbox 0x00020920
void dispatch_runtime_body_position_event(const Vec3& position, i32 id,
    RuntimeBodyActionMessages& context); // Xbox 0x0013AD60
void request_runtime_body_action_6e(std::span<u8> body, const Vec3& position,
    RuntimeBodyActionMessages& context); // Xbox 0x00020980
void request_runtime_body_action_7b(std::span<u8> body, std::span<u8> message,
    const EntityBitBlock& flags, RuntimeBodyActionMessages& context); // Xbox 0x000209E0
struct RuntimePlayerCollisionReply
{
    u32 id;
    u32 accepted;
    u32 handled;
    const Vec3* start;
    Vec3* target;
    const Vec3* position;
    u8* contact;
    u32 pass;
};
struct RuntimePlayerCollisionAction
{
    std::function<void(RuntimePlayerCollisionReply&)> message; // Action vtable +0xC.
};
bool notify_runtime_player_collision_action(const Vec3& start, Vec3& target,
    const Vec3& position, u8* contact, u32 pass,
    RuntimePlayerCollisionAction*& action); // Xbox 0x00075960
}
