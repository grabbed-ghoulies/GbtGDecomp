#pragma once
#include "player_attachment_helpers.h"
#include "body_attachment_cleanup.h"
#include "player_mind_setup.h"
#include "actor_pose_updates.h"

namespace gbtg
{
struct RuntimePlayerAttachmentActor
{
    RuntimeBodyAttachedActor& object;
    Actor& actor;
    RuntimeBodyAnimationReceiver& receiver;
    ActorAnimationState& animation;
    ModelAnimationState& model;
    u16 bone;
    const SkeletonDispatchContext* callbacks;
    ModelNodeFlagCache& cache;
    const AnimationMatrix& reset;
};
struct RuntimePlayerAttachment
{
    std::span<u8> body;
    RuntimeBodyAttachedActor& host;
    RuntimeBodyActorAttachment& ownership;
    RuntimeBodyActionSelection& actions;
    RuntimeBodyAttachedActor*& secondary; // Body +0x4C.
    RuntimeBodyAttachedActor*& mind_action; // Actor +0x958 -> mind +0x18.
    bool& mind_present; // Actor +0x958's native null test.
    ActorAnimationState& host_animation;
    const std::unordered_map<RuntimeBodyAttachedActor*, RuntimePlayerAttachmentActor*>& actors;
    const char* const* weapon_names; // DAT_00434104.
    const char* const* class_names; // DAT_004340EC.
    std::function<u16()> body_action_key; // Actual body vtable +0x20.
};
void format_runtime_body_variant(std::span<u8> body, const char* format,
    ...); // Xbox 0x00020B30
bool attach_runtime_player_actor(RuntimePlayerAttachmentActor& attached, i32 update_action,
    RuntimePlayerAttachment& context); // Xbox 0x0001B2B0
bool release_runtime_player_attachment(i32 force, u32 reason,
    RuntimePlayerAttachment& context); // Xbox 0x0001B810
bool replace_runtime_player_attachment(RuntimePlayerAttachment& context); // Xbox 0x0013A7C0
struct RuntimePlayerAttachmentCreation
{
    RuntimePlayerAttachment& attachment;
    RuntimePlayerChildContext& child;
    const std::unordered_map<const u8*, RuntimePlayerAttachmentActor*>& created;
};
FactoryObject create_runtime_player_attachment(const char* name, i32 replace,
    RuntimePlayerAttachmentCreation& context); // Xbox 0x0013A720
}
