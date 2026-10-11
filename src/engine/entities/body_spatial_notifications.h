#pragma once
#include "actor_live_attachments.h"

namespace gbtg
{
struct RuntimeBodySpatialNotifications
{
    Actor& actor;
    u32 actor_address;
    u8 channel; // Actor +0xCEC low byte.
    entity_detail::ListHead& actor_head; // Actor +0x8F4.
    entity_detail::ListHead& global_head; // DAT_00545984.
};
void notify_runtime_body_spatial_shape(std::span<const u8> body,
    std::span<u8, 0x20> scratch, RuntimeBodySpatialNotifications& context); // Xbox 0x0001D780
void capture_runtime_body_spatial_pose(std::span<u8> body,
    std::span<u8, 0x20> scratch, RuntimeBodySpatialNotifications& context); // Xbox 0x000257C0
}

