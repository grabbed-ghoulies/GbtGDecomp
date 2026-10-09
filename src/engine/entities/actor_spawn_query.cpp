#include "actor_spawn_query.h"
#include "actor_spawn_eligibility.h"
#include "shape_transform.h"
#include "entity.h"
#include "../navigation/navigation.h"
#include "actor_transform.h"

#include <bit>
#include <cmath>

namespace gbtg
{
// Xbox 0x001476D0
bool select_actor_spawn_point(const Actor& actor, const Vec3& query, Vec3& point,
    float& radius, i32& index)
{
    if (actor.spawn_point_count != 0)
    {
        const EntityBonePose* pose;
        if (actor.model == nullptr)
        {
            pose = nullptr;
        }
        else
        {
            pose = actor.model->alternate_shape_pose;
            if (pose == nullptr)
            {
                pose = actor.model->primary_shape_pose;
            }
        }
        index = 0;
        radius = shape_sphere_metrics(*actor.spawn_points[0], actor.xform, pose, point);
        i32 candidate = 1;
        if (actor.spawn_point_count > 1)
        {
            do
            {
                Vec3 center;
                const float reach = shape_sphere_metrics(*actor.spawn_points[candidate], actor.xform, pose, center);
                const float dx = query.x - center.x;
                const float dy = query.y - center.y;
                const float dz = query.z - center.z;
                const float kept_x = query.x - point.x;
                const float kept_y = query.y - point.y;
                const float kept_z = query.z - point.z;
                if (dz * dz + dy * dy + dx * dx < kept_x * kept_x + kept_z * kept_z + kept_y * kept_y)
                {
                    index = candidate;
                    radius = reach;
                    point = center;
                }
                ++candidate;
            }
            while (candidate < actor.spawn_point_count);
        }
        return true;
    }
    return false;
}

// Xbox 0x00129CD0
bool actor_spawn_path_clear(const Actor& actor, const Vec3& query, u8 flags)
{
    if ((flags & 8) == 0)
    {
        return true;
    }
    Vec3 point;
    float radius;
    i32 point_index;
    select_actor_spawn_point(actor, query, point, radius, point_index);
    i32 last;
    if (entity_detail::navigation_path_clear(query, point, 0, 0, 0, 0, nullptr, &last, 0))
    {
        const i32 floor = entity_detail::navigation_floor_index(point, -1, 0);
        const i32 cell = entity_detail::navigation_cell_index(floor, point);
        if (cell != -1 && !entity_detail::navigation_cell_is_isolated(*entity_detail::g_nav_grids[floor], u32(cell), true))
        {
            return true;
        }
    }
    return false;
}

// Xbox 0x00129D70
Actor* find_nearest_spawn_actor(const Vec3& query, const Actor* requester, float* distance, u8 flags)
{
    Actor* best = nullptr;
    float closest = std::bit_cast<float>(0x7F7FFFFFu);
    auto* link = static_cast<ActorSpawnLink*>(entity_detail::g_world.body_b.next);
    if (link == nullptr)
    {
        return nullptr;
    }
    do
    {
        Actor& actor = *link->actor;
        if (actor_matches_spawn_filter(actor, requester, flags))
        {
            const Vec3 position = actor_world_position(actor);
            const float dy = query.y - position.y;
            // COMISS/JA passes unordered vertical comparisons.
            if (!(dy * dy > std::bit_cast<float>(0x441C4000u)))
            {
                const float dx = query.x - position.x;
                const float dz = query.z - position.z;
                const float squared = dx * dx + dz * dz + dy * dy;
                if (squared < closest && actor_spawn_path_clear(actor, query, flags))
                {
                    best = &actor;
                    closest = squared;
                }
            }
        }
        link = link->next;
    }
    while (link != nullptr);
    if (best != nullptr && distance != nullptr)
    {
        *distance = float(std::sqrt(double(closest)));
    }
    return best;
}
}
