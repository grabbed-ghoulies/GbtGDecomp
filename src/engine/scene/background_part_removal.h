#pragma once

#include "background_volumes.h"
#include "../entities/actor_attachment_events.h"
#include "../entities/physical_shape_events.h"

namespace gbtg
{
struct BackgroundPartRemoval
{
    std::span<BackgroundAnimationVolume> volumes;
    std::span<const u8> header;
    const entity_detail::SegmentProbe* first_shape_object; // DAT_00511AF4.
    const std::function<const Actor&(const entity_detail::SegmentProbe&)>& actor_of;
    const std::unordered_map<const u8*, PhysicalShapeEventList>& shape_lists;
    const u8* attachment_records; // Actor +0x8C contents.
    ActorAttachmentEventContext& attachments;
};
void remove_background_part_volume(Actor& actor, i32 row, u32 value,
    Vec3& position, BackgroundPartRemoval& context); // Xbox 0x00044EF0
}
