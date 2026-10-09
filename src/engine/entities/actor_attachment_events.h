#pragma once

#include "actor_attachment_requests.h"
#include "gbtg/actor_spawn_factory.h"

namespace gbtg
{
struct ActorAttachmentEventContext
{
    Actor& actor;
    ActorAttachmentList& attachments;
    ActorEffectStreamContext& effects;
    ObjectFactoryRegistry& factory;
    std::span<ExecutableResourcePool> resources;
    std::vector<FactoryObject>& objects; // host ownership of factory allocations
    u32 actor_address; // original data pointer at parameters+8, resolved by the factory's native views
    const std::unordered_map<const u8*, const u8*>& linked_records; // relocated record+0xC8 pointers
};
using ActorAttachmentEventCallback = void (*)(const u8*, const RuntimeActorCreationOptions*,
    ActorAttachmentEventContext&, u32);
void request_attachment_joint_event(const u8* record, const RuntimeActorCreationOptions* options,
    ActorAttachmentEventContext& context); // Xbox 0x0003BBF0
void request_attachment_binding_event(const u8* record, const RuntimeActorCreationOptions* options,
    ActorAttachmentEventContext& context); // Xbox 0x0003BC20
void request_attachment_index_event(const u8* record, const RuntimeActorCreationOptions* options,
    ActorAttachmentEventContext& context); // Xbox 0x0003BC40
void request_attachment_camera_effect(const u8* record, const RuntimeActorCreationOptions* options,
    ActorAttachmentEventContext& context); // Xbox 0x0003BC70
void request_attachment_resource_object(const u8* record,
    ActorAttachmentEventContext& context); // Xbox 0x0003BCE0
void request_attachment_camera_shake(const u8* record, const RuntimeActorCreationOptions* options,
    ActorAttachmentEventContext& context); // Xbox 0x0003BD10
void request_attachment_sound_effect(const u8* record, const RuntimeActorCreationOptions* options,
    ActorAttachmentEventContext& context); // Xbox 0x0003BD80
void dispatch_actor_attachment_event(const u8* record, const RuntimeActorCreationOptions* options,
    ActorAttachmentEventContext& context, u32 value); // Xbox 0x0003BEC0
i32 dispatch_actor_attachment_records(const u8* record, i32 id,
    ActorAttachmentEventCallback callback, const RuntimeActorCreationOptions* options,
    ActorAttachmentEventContext& context, u32 value); // Xbox 0x0003BE20
void dispatch_actor_resource_callout(const RuntimeActorResourceReference& resource, i32 id,
    const RuntimeActorCreationOptions* options,
    ActorAttachmentEventContext& context); // Xbox 0x0003C040
}
