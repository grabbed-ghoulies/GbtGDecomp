#include "actor_collision_culling.h"
#include "actor_runtime_queries.h"
#include "actor_transform.h"
#include <cmath>
#include <bit>

namespace gbtg
{
// Xbox 0x000559F0
bool runtime_collision_shape_list_has_physical_shape(std::span<const u8> list)
{
    const i32 count = std::bit_cast<i32>(read_u32(list, 0));
    size_t offset = 0x10;
    for (i32 index = 0; index < count; ++index)
    {
        if (read_u32(list, offset + 0xC) == 0x1235 && list[offset] < 3)
        {
            return true;
        }
        offset += read_u32(list, offset + 8);
    }
    return false;
}

// Xbox 0x0015A270
bool runtime_collision_spheres_overlap(const Vec3& first, float first_radius,
    const Vec3& second, float second_radius)
{
    const float dx = first.x - second.x;
    const float dz = first.z - second.z;
    const float dy = first.y - second.y;
    const float radius = first_radius + second_radius;
    return !((dx * dx + dz * dz) + dy * dy > radius * radius);
}

namespace
{
Vec3 midpoint(const Vec3& start, const Vec3& end)
{
    return {(end.x - start.x) * .5f + start.x,
        (end.y - start.y) * .5f + start.y,
        (end.z - start.z) * .5f + start.z};
}
long double half_length(const Vec3& start, const Vec3& center)
{
    const float dx = start.x - center.x;
    const float dy = start.y - center.y;
    const float dz = start.z - center.z;
    return std::sqrt((static_cast<long double>(dz) * dz + static_cast<long double>(dy) * dy)
        + static_cast<long double>(dx) * dx);
}
}

// Xbox 0x0003E4A0
bool runtime_actor_culls_segment(const Actor& actor, std::span<const u8> bytes,
    const Vec3& start, const Vec3& end)
{
    const float radius = runtime_actor_view_radius(actor, bytes);
    const Vec3 position = actor_world_position(actor);
    const Vec3 center = midpoint(start, end);
    const float extent = float(half_length(start, center));
    return !runtime_collision_spheres_overlap(position, radius, center, extent);
}

// Xbox 0x0003E590
bool runtime_actor_culls_swept_sphere(const Actor& actor, std::span<const u8> bytes,
    const Vec3& start, const Vec3& end, float radius)
{
    const float actor_radius = runtime_actor_view_radius(actor, bytes);
    const Vec3 position = actor_world_position(actor);
    const Vec3 center = midpoint(start, end);
    const float extent = float(half_length(start, center) + static_cast<long double>(radius));
    return !runtime_collision_spheres_overlap(position, actor_radius, center, extent);
}

// Xbox 0x0003E680
bool runtime_actor_culls_sphere(const Actor& actor, std::span<const u8> bytes,
    const Vec3& center, float radius)
{
    const float actor_radius = runtime_actor_view_radius(actor, bytes);
    const Vec3 position = actor_world_position(actor);
    return !runtime_collision_spheres_overlap(position, actor_radius, center, radius);
}
}
