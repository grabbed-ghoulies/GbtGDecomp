#pragma once

#include "../graphics/actor_effect_stream.h"

namespace gbtg
{
void clear_actor_attachment_effects(ActorAttachmentList& active,
    ActorAttachmentNode& pending); // Xbox 0x0003D0A0
ActorAttachmentNode* create_actor_joint_attachment(ActorAttachmentList& list,
    const char* name, u32 flags, i16 joint, const RuntimeActorCreationOptions* options); // Xbox 0x0003D250
ActorAttachmentNode* create_actor_binding_attachment(Actor& actor, ActorAttachmentList& list,
    const char* name, u32 flags, i16 kind, const RuntimeActorCreationOptions* options); // Xbox 0x0003D330
bool start_actor_attachment_effect(Actor& actor, ActorAttachmentNode& node,
    ActorEffectStreamContext& context); // Xbox 0x0003D3A0
void update_actor_attachment_effects(Actor& actor, ActorAttachmentList& active,
    ActorAttachmentNode& pending, ActorEffectStreamContext& context); // Xbox 0x0003CD00
u16 request_actor_joint_attachment(Actor& actor, ActorAttachmentList& list, const char* name,
    u32 flags, i16 joint, const RuntimeActorCreationOptions* options,
    ActorEffectStreamContext& context); // Xbox 0x0003CE40
u16 request_actor_binding_attachment(Actor& actor, ActorAttachmentList& list, const char* name,
    u32 flags, i16 kind, const RuntimeActorCreationOptions* options,
    ActorEffectStreamContext& context); // Xbox 0x0003CEB0
u16 request_actor_index_attachment(Actor& actor, ActorAttachmentList& list, const char* name,
    u32 flags, i32 index, const RuntimeActorCreationOptions* options,
    ActorEffectStreamContext& context); // Xbox 0x0003CF30
RuntimeActorCreationOptions& actor_world_creation_options(Actor& actor); // Xbox 0x0013B6B0
}
