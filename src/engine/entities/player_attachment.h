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
// 0x0001B2B0 with the bind applied at once (0x0003B4C0 / 0x0003B460 at `origin` / `euler`, DAT_0050DD80 /
// DAT_0050DD74): EAX the attached actor, the body and the update flag on the stack.
bool attach_runtime_player_weapon(RuntimePlayerAttachmentActor& attached, i32 update_action,
    RuntimePlayerAttachment& context, const Vec3& origin, const Vec3& euler); // Xbox 0x0001B190
bool attach_runtime_player_secondary(RuntimePlayerAttachmentActor& attached,
    RuntimePlayerAttachment& context); // Xbox 0x0001B3E0
bool release_runtime_player_attachment(i32 force, u32 reason,
    RuntimePlayerAttachment& context); // Xbox 0x0001B810
bool replace_runtime_player_attachment(RuntimePlayerAttachment& context); // Xbox 0x0013A7C0
struct RuntimePlayerAttachmentCreation
{
    RuntimePlayerAttachment& attachment;
    RuntimePlayerChildContext& child;
    // A just-created object's attachment view (0x0001B2B0's EAX), before the scene keeps the object.
    std::function<RuntimePlayerAttachmentActor&(FactoryObject&)> view_of;
};
FactoryObject create_runtime_player_attachment(const char* name, i32 replace,
    RuntimePlayerAttachmentCreation& context); // Xbox 0x0013A720
FactoryObject command_runtime_body_create_attachment(const char* name,
    RuntimePlayerAttachmentCreation& context); // Xbox 0x0001A6D0
FactoryObject command_runtime_body_create_secondary(std::span<const u8> command,
    std::span<char, 128> name_scratch,
    const std::function<void(RuntimeBodyAttachedActor&, std::span<char, 128>)>& query_name,
    RuntimePlayerAttachmentCreation& context); // Xbox 0x0001A2D0
}
