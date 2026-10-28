#pragma once
#include "body_damage_dispatch.h"

namespace gbtg
{
u32 receive_runtime_body_collision_hit(std::span<u8> body,
    RuntimeBodyAttachedActor& source, std::span<const u8> source_body,
    std::span<const u8> actor_bytes, const Vec3& normal, const Vec3& position,
    u32 mode, u8 joint, u32& handled,
    RuntimeBodyDamageEntry& context); // Xbox 0x000202E0
void reflect_runtime_body_timer_hit(std::span<const u8> body,
    RuntimeBodyAttachedActor& actor, std::span<u8> source_body,
    std::span<u8> damage, RuntimeBodyDamageEntry& source_context,
    RuntimeBodyDamageEntry& reply_context); // Xbox 0x00020A60
struct RuntimeBodyContactHitViews
{
    const entity_detail::SegmentPool& contacts;
    u32 contacts_address;
    RuntimeBodyAttachedActor& source;
    RuntimeBodyAttachedActor* retained_source; // Native view of body +0x270 when +0x278 is zero.
};
void receive_runtime_body_projectile_contact(std::span<u8> body,
    std::span<u8> message, const RuntimeBodyContactHitViews& views,
    RuntimeBodyDamageEntry& context); // Xbox 0x00022CF0
void receive_runtime_body_moving_contact(std::span<u8> body,
    std::span<u8> message, const RuntimeBodyContactHitViews& views,
    RuntimeBodyDamageEntry& context); // Xbox 0x00022E40
void notify_runtime_body_impact_position(const Vec3& position, u32 blocked,
    RuntimeBodyActionMessages& context); // Xbox 0x0001F150
void receive_runtime_body_impact(std::span<u8> body, std::span<u8> message,
    const RuntimeBodyContactHitViews& views, const u8* tag_rows,
    const char* const* tag_names,
    RuntimeBodyDamageEntry& context); // Xbox 0x000231C0
void receive_runtime_body_timer_contact(std::span<u8> body,
    std::span<const u8> message, RuntimeBodyAttachedActor& source,
    std::span<const u8> bound,
    RuntimeBodyDamageEntry& context); // Xbox 0x00022A40
void request_runtime_body_timer_contact(std::span<u8> body,
    std::span<const u8> message, RuntimeBodyAttachedActor& source,
    std::span<const u8> source_body, std::span<const u8> bound,
    RuntimeBodyDamageEntry& context); // Xbox 0x000229F0
// A vampire's (type 0x2C) moving contact from in front: a reaction hit (0xC) the sender's class and the
// actor's tables size, which as a lighter hit (0xD) also sends the actor message 0x1A while bit block byte 8 bit 0 is clear.
void receive_runtime_body_front_hit(std::span<u8> body, std::span<u8> message,
    const RuntimeBodyContactHitViews& views, RuntimeBodyDamageEntry& context); // Xbox 0x00022FB0
void receive_runtime_body_contact_health_hit(std::span<u8> body,
    std::span<u8> message, const RuntimeBodyContactHitViews& views,
    RuntimeBodyDamageEntry& context); // Xbox 0x000233A0
void receive_runtime_body_contact_knockdown(std::span<u8> body,
    std::span<u8> message, const RuntimeBodyContactHitViews& views,
    RuntimeBodyDamageEntry& context); // Xbox 0x00023580
void runtime_body_damage_contact_position(std::span<const u8> damage,
    const entity_detail::SegmentPool* contacts, Vec3& output); // Xbox 0x00021010
void notify_runtime_body_lethal_hit_position(const Vec3& position,
    RuntimeBodyActionMessages& context); // Xbox 0x0001F1D0
void notify_runtime_body_contact_hit_position(const Actor* source, const Vec3& position,
    u32 knockdown, u32 blocked, std::span<const u8> attributes,
    RuntimeBodyActionMessages& context); // Xbox 0x0001F240
void dispatch_runtime_body_damage_reply(std::span<const u8> damage,
    std::span<const u32, 4> result,
    RuntimeBodyDamageEntry& context); // Xbox 0x00020F80
void dispatch_runtime_body_scaled_position_event(const Vec3& position, i32 id,
    float scale, RuntimeBodyActionMessages& context); // Xbox 0x0013ADC0
void notify_runtime_body_hit_position(const Actor* source, const Vec3& position,
    u32 blocked, std::span<const u8> attributes,
    RuntimeBodyActionMessages& context); // Xbox 0x0001F0B0
void receive_runtime_body_direct_hit(std::span<u8> body,
    RuntimeBodyAttachedActor& source, const Vec3& notice_position,
    RuntimeBodyDamageEntry& context); // Xbox 0x000205B0
void receive_runtime_body_knock_back(std::span<u8> body, const Actor& actor,
    std::span<const u8> actor_bytes, RuntimeBodyAttachedActor* background,
    RuntimeBodyDamageEntry& context); // Xbox 0x00020690
void receive_runtime_body_health_notice(std::span<u8> body, std::span<u8> message,
    std::span<const char* const, 4> first_names,
    std::span<const char* const, 4> second_names,
    RuntimeBodyDamageEntry& context); // Xbox 0x00023740
void request_runtime_body_lethal_action(std::span<u8> body,
    RuntimeBodyActionSelection& actions); // Xbox 0x00134E50
}
