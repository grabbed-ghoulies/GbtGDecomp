#pragma once

#include "gbtg/entity.h"
#include "../entities/actor_live_attachments.h"
#include "../entities/actor_model_anchors.h"

namespace gbtg
{
struct BackgroundAnimationVolume
{
    std::array<u8, 0x9C> fields; // original background +0x94C row
    ActorLiveAttachment* attachment; // native view of row +0x14
    ActorModelAnchor* anchor; // native view of row +0x18
};
bool build_background_animation_volume(const Actor& actor, i32 index,
    std::span<const u8> animation_header, std::array<u8, 0x20>& output,
    u16& tag); // Xbox 0x00045F70
void initialize_background_animation_volume(Actor& actor, i32 index,
    std::span<const u8> animation_header, std::span<BackgroundAnimationVolume> records,
    entity_detail::ListHead& attachment_owner, entity_detail::ListHead& attachment_global,
    entity_detail::ListHead& anchor_owner, const Vec3& origin); // Xbox 0x00045D70
}
