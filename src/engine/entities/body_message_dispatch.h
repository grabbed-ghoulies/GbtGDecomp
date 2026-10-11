#pragma once
#include "body_animation_dispatch.h"
#include "body_spatial_notifications.h"
#include "body_timers.h"

namespace gbtg
{
struct RuntimeBodyMessageChild
{
    std::function<void(std::span<u8>)> message; // Actual child vtable +0xC.
};
struct RuntimeBodyMessageViews
{
    std::span<const u8> animation_command; // Message +0xC.
    RuntimeSegmentContactActor* contact; // Message +4, ids 6/0x7B.
    Actor* referenced_actor; // Message +4, ids 0x47/0x7C.
    const EntityCylinderBound* cylinder; // Message +4, id 0x63.
};
struct RuntimeBodyMessageDispatch
{
    Actor& actor;
    std::span<u8>& actor_bytes;
    RuntimeBodyActionMessages& actions;
    const std::unordered_map<const u8*, RuntimeBodyMessageChild*>& children;
    RuntimeBodyAnimationDispatch& animation;
    RuntimeBodySpatialNotifications& spatial;
    std::span<u8, 0x20> shape_scratch;
    entity_detail::LatchNode*& latches;
    RuntimeBodyCollisionState*& collision;
    RuntimeBodyTimerRecipient*& first_timer; // Native view of body +0x760.
    RuntimeBodyTimerRecipient*& second_timer; // Native view of body +0x7F0.
    const entity_detail::ListHead& players;
    const Vec3& default_position; // DAT_0050DD80.
};
void dispatch_runtime_parent_body_message(std::span<u8> body, std::span<u8> message,
    const RuntimeBodyMessageViews& views,
    RuntimeBodyMessageDispatch& context); // Xbox 0x000253B0
}
