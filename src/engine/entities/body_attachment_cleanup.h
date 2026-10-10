#pragma once
#include "body_animation_setup.h"
#include "body_action_selection.h"
#include "actor_bind_tree.h"
#include "actor_live_attachments.h"

namespace gbtg
{
struct RuntimeBodyAttachmentLists
{
    ActorAttachmentNode*& first; // Actor +0x1B8.
    ActorAttachmentNode*& second; // Actor +0x1B0.
    std::span<u8*> timed; // DAT_00506930.
};
bool remove_runtime_body_attachment(u16 token, RuntimeBodyAttachmentLists& lists); // Xbox 0x0003CFD0
void clear_runtime_body_attachments(std::span<u8> body,
    RuntimeBodyAttachmentLists& lists); // Xbox 0x00025090
struct RuntimeBodyActorAttachment
{
    RuntimeBodyAttachedActor*& attached; // Body +0x48.
    u32& actor_address; // Body +8.
    Actor& actor;
    RuntimeBodyBitStorage*& flags; // Body +0x15C.
    RuntimeBodyAnimationReceiver*& receiver; // Body +0x10, also cleared on detach.
};
RuntimeBodyAttachedActor* detach_runtime_body_actor(std::span<u8> body,
    RuntimeBodyActorAttachment& context); // Xbox 0x0001B4C0
struct RuntimeParentBodyCleanup
{
    RuntimeBodyAnimationSetup& animation;
    RuntimeBodyActorAttachment& attachment;
    std::span<std::span<const u8>, 10> resources;
    RuntimeBodyResourceHash*& hashes;
    u32& hash_count;
    RuntimeBodyAttachmentLists& lists;
    RuntimeBodyCollisionState*& collision;
    std::span<const u8>& attributes; // Body +0x268.
    RuntimeBodyShapeStorage& shapes;
    RuntimeBodyHealth& health;
    FactoryObject& first_action;
    FactoryObject& second_action;
    std::span<const u8>& actions;
    ObjectFactoryRegistry& factory;
};
void destroy_runtime_parent_body(std::span<u8> body,
    RuntimeParentBodyCleanup& context); // Xbox 0x00024F70
}
