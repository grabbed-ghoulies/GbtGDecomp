#pragma once
#include "actor_base_messages.h"
#include "actor_frame_update.h"
#include "../../gbtg/runtime/object_lifecycle.h"

namespace gbtg
{
struct ActorFrameReleaseContext
{
    ObjectFactoryRegistry& factory;
    const std::unordered_map<ActorFrameAttachment*, FactoryObject*>& objects;
    std::function<void(const ActorReferenceMessage&)> message; // Actual actor vtable+0xC.
};
struct ActorReleaseContext
{
    RuntimeActorStartupState& state;
    RuntimeActorStartupContext& startup;
    ActorRenderRegistration& render;
    ActorFrameReleaseContext& frames;
    ActorFrameAttachment*& frame_head;
    ActorAttachmentNode& pending;
    entity_detail::ListHead& anchors;
    entity_detail::ListHead& live_attachments;
    void*& allocation_8fc;
    void*& matrices;
    RuntimeActorSoundQueues& sounds;
    ModelAnimationState& pose;
    const char* empty_name; // DAT_00546208.
};
void release_actor_frame_attachments(ActorFrameAttachment* head,
    ActorFrameReleaseContext& context); // Xbox 0x0003DE90
void release_runtime_actor(ActorReleaseContext& context); // Xbox 0x0003EBA0
}
