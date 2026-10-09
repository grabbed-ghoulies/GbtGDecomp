#pragma once

#include "actor_event_messages.h"
#include "actor_attachment_events.h"
#include "actor_model_anchors.h"
#include "actor_render_registration.h"
#include "actor_live_attachments.h"

namespace gbtg
{
struct ActorVectorMessage
{
    u32 id;
    Vec3 value; // +4
};
struct ActorReferenceMessage
{
    u32 id;
    void* value; // +4
};
struct ActorAnimationMixMessage
{
    u32 id;
    u32 slot; // +4
    AnimationMix mix; // +8
    const char* name; // string at +B8
};
struct ActorBaseContext
{
    ActorEventContext& events;
    ActorRenderRegistration& render;
    ActorAttachmentEventContext& callouts;
    entity_detail::ListHead& model_anchors;
    entity_detail::ListHead& live_attachments;
    ActorLink& world_bind; // native view of DAT_00511B10, including its +8 child
    void*& watched_actor; // actor +11C
    ModelAnimationState& pose;
    const SkeletonDispatchContext* skeleton;
    ModelNodeFlagCache& node_cache;
    const AnimationMatrix& reset;
};
void dispatch_base_actor_message(const void* message, ActorBaseContext& context); // Xbox 0x0003EF70
}
