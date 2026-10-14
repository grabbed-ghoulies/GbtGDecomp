#include "actor_view_visibility.h"
#include "actor_runtime_queries.h"
#include "actor_shape_bounds.h"
#include "actor_transform.h"
#include "shape_transform.h"
#include "../camera/view_frustum.h"

namespace gbtg
{
// Xbox 0x0003C830
u32 runtime_actor_in_view(RuntimeActorViewVisibility& context, i32 pass, i32 view)
{
    const Actor& actor = context.actor;
    const Vec3 world = actor_world_position(actor);
    float radius = runtime_actor_view_radius(actor, context.actor_bytes);
    if (i32(read_u32(context.actor_bytes, 0x28C)) > 0)
    {
        if (context.matrices == nullptr)
        {
            throw Error("actor view visibility has no +0x288 matrix setup");
        }
        const float reach = runtime_actor_matrix_reach(actor, *context.matrices);
        if (reach > radius)
        {
            radius = reach;
        }
    }
    const std::span<const float, 42> frustum(context.view.frustum);
    if (radius != 0.f || radius != radius)
    {
        u8& hint = context.actor_bytes[0x184 + size_t(pass) * 4 + size_t(view)];
        if (classify_runtime_sphere_frustum(frustum, world, radius, hint) != 0)
        {
            return 1;
        }
    }
    if ((runtime_actor_physical_shape_flags(actor) & 0x200) == 0)
    {
        return 0;
    }
    const EntityBonePose* pose = nullptr;
    if (actor.model != nullptr)
    {
        pose = actor.model->alternate_shape_pose;
        if (pose == nullptr)
        {
            pose = actor.model->primary_shape_pose;
        }
    }
    // The caller's pass argument slot is reused as the shape walk's plane hint, starting at 0.
    u8 hint = 0;
    const u8* shape = find_runtime_actor_shape(actor, 0x200, 0, -1);
    while (shape != nullptr)
    {
        Vec3 center;
        const float extent = shape_sphere_metrics(*reinterpret_cast<const EntityShapeNode*>(shape), actor.xform,
            pose, center);
        if (classify_runtime_sphere_frustum(frustum, center, extent, hint) != 0)
        {
            return 1;
        }
        // 0x0003C962..0x0003C9B9 inline 0x000425D0.
        shape = next_runtime_actor_shape(actor, shape, 0x200, 0, -1);
    }
    return 0;
}
}
