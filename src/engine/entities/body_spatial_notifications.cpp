#include "body_spatial_notifications.h"
#include "actor_shape_bounds.h"
#include "actor_transform.h"
#include "entity_heap.h"
#include "shape_copy.h"
#include "body_fields.h"
#include <cstring>

namespace gbtg
{
// Xbox 0x0001D780
void notify_runtime_body_spatial_shape(std::span<const u8> body,
    std::span<u8, 0x20> scratch, RuntimeBodySpatialNotifications& context)
{
    const Vec3 position = actor_world_position(context.actor);
    const float height = context.actor.radius_y * get(body, body_field::height_scale);
    const float radius = context.actor.radius_xz * get(body, body_field::radius_scale);
    std::memcpy(scratch.data() + 4, &position, 12);
    const u32 zero = 0;
    std::memcpy(scratch.data() + 0x10, &zero, 4);
    std::memcpy(scratch.data() + 0x14, &height, 4);
    std::memcpy(scratch.data() + 0x18, &zero, 4);
    std::memcpy(scratch.data() + 0x1C, &radius, 4);
    scratch[0] = 4;
    scratch[1] = 0x20;
    scratch[2] = 1;
    allocate_actor_live_attachment(context.actor, context.actor_head, context.global_head,
        0, context.channel, read_u16(body, 0x6E), scratch.data(), actor_transform_origin);
}

// Xbox 0x000257C0
void capture_runtime_body_spatial_pose(std::span<u8> body,
    std::span<u8, 0x20> scratch, RuntimeBodySpatialNotifications& context)
{
    const Vec3 position = actor_world_position(context.actor);
    std::memcpy(body.data() + 0x18, &position, 12);
    const Vec3 euler = actor_world_euler(context.actor);
    std::memcpy(body.data() + 0x72C, &euler, 12);
    if ((runtime_actor_physical_shape_flags(context.actor) & 2) != 0)
    {
        notify_runtime_body_spatial_shape(body, scratch, context);
    }
}
}

