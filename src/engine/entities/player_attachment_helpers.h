#pragma once
#include "body_action_selection.h"

namespace gbtg
{
struct RuntimePlayerAttachmentEligibility
{
    std::span<const u8> body;
    const EntityBitBlock& flags;
    Actor& body_actor;
    RuntimeBodyAttachedActor* attached;
};
bool runtime_player_body_allows_attachment(const RuntimePlayerAttachmentEligibility& body,
    const Actor& target); // Xbox 0x00027DA0
float runtime_player_body_attachment_radius(
    const RuntimePlayerAttachmentEligibility& body); // Xbox 0x0001D3D0
bool runtime_player_attachment_is_in_range(const RuntimePlayerAttachmentEligibility& body,
    const Actor& target); // Xbox 0x00027E50
bool runtime_player_can_attach(const RuntimePlayerAttachmentEligibility& body,
    const RuntimeBodyAttachedActor& target, std::span<const u8> target_link); // Xbox 0x00027D40
bool runtime_player_body_target_is_near(const RuntimePlayerAttachmentEligibility& body,
    std::span<const u8> target_body, const Actor& target_actor); // Xbox 0x00134240
bool runtime_player_body_point_is_near(const RuntimePlayerAttachmentEligibility& body,
    std::span<const u8> target); // Xbox 0x00134350
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
