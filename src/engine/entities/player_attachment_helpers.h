#pragma once
#include "body_action_selection.h"

namespace gbtg
{
i32 runtime_attachment_class_value(std::span<const u8> attached,
    u16 host_class); // Xbox 0x00146190
void set_runtime_attachment_host(RuntimeBodyAttachedActor& attached,
    const RuntimeBodyAttachedActor& host); // Xbox 0x001461C0
bool runtime_attachment_available(std::span<const u8> attached,
    const RuntimeBodyAttachedActor* host); // Xbox 0x00146100
ActorBind* ensure_runtime_actor_bind(Actor& actor); // Xbox 0x0003B9D0
void bind_runtime_actor_joint(Actor& actor, Actor* parent,
    i32 joint_key, u8 flags); // Xbox 0x0003B840
void notify_runtime_attachment_host(RuntimeBodyAttachedActor& host,
    RuntimeBodyAttachedActor& attached); // Xbox 0x0001B770
void notify_runtime_secondary_attachment_host(RuntimeBodyAttachedActor& host,
    RuntimeBodyAttachedActor& attached); // Xbox 0x0001B7C0
}
