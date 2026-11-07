#include "mind_escape_position.h"
#include "../entities/mind_fields.h"
#include "../entities/player_mind_targets.h"
#include "../scene/scene_markers.h"
#include "../entities/actor_transform.h"
#include "gbtg/anim.h"
#include "../entities/player_fields.h"
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
Vec3 marker_position(const u8* row)
{
    Vec3 position;
    std::memcpy(&position, row + 0xC, sizeof(position));
    return position;
}
float distance_to_target(const u8* row, std::span<const u8> target)
{
    const Vec3 point = marker_position(row);
    const float x = point.x - mind_target_record(target).shape.position.x;
    const float y = point.y - mind_target_record(target).shape.position.y;
    const float z = point.z - mind_target_record(target).shape.position.z;
    return (z * z + y * y) + x * x;
}
}

// Xbox 0x00135B60
bool select_runtime_mind_escape_position(RuntimeMindEscapePosition& context,
    float heading, float minimum, float maximum, Vec3& output)
{
    if (player_record(context.actor_bytes).mind == 0 || context.target.data() == nullptr)
    {
        return false;
    }
    Vec3 position = actor_world_position(context.actor);
    i32 count = count_runtime_markers(0x1B, context.markers);
    const u8* chosen = nullptr;
    if (count != 0)
    {
        Vec3 direction{mind_target_record(context.target).shape.position.x - position.x,
            mind_target_record(context.target).shape.position.y - position.y,
                mind_target_record(context.target).shape.position.z - position.z};
        const float length = (direction.y * direction.y + direction.z * direction.z) + direction.x * direction.x;
        const u8* farthest = nullptr;
        if (length != 0.f)
        {
            const long double reciprocal = retail_reciprocal_sqrt(length);
            direction.x = float(static_cast<long double>(direction.x) * reciprocal);
            direction.y = float(static_cast<long double>(direction.y) * reciprocal);
            direction.z = float(static_cast<long double>(direction.z) * reciprocal);
        }
        i32 behind = 0;
        u16 id = next_runtime_marker_id(0, 0x1B, 1, context.markers);
        if (count > 0)
        {
            do
            {
                const u8* row = find_runtime_marker(context.markers, i16(id));
                if (farthest == nullptr || distance_to_target(farthest, context.target)
                    < distance_to_target(row, context.target))
                {
                    farthest = row;
                }
                const Vec3 marker = marker_position(row);
                Vec3 offset{marker.x - position.x, marker.y - position.y, marker.z - position.z};
                const float square = (offset.z * offset.z + offset.y * offset.y) + offset.x * offset.x;
                if (square != 0.f)
                {
                    const long double reciprocal = retail_reciprocal_sqrt(square);
                    offset.x = float(static_cast<long double>(offset.x) * reciprocal);
                    offset.y = float(static_cast<long double>(offset.y) * reciprocal);
                    offset.z = float(static_cast<long double>(offset.z) * reciprocal);
                }
                if ((offset.y * direction.y + offset.z * direction.z) + offset.x * direction.x < 0.f)
                {
                    ++behind;
                    if (chosen == nullptr || distance_to_target(chosen, context.target)
                        < distance_to_target(row, context.target))
                    {
                        chosen = row;
                    }
                }
                id = next_runtime_marker_id(id, 0x1B, 0, context.markers);
                --count;
            }
            while (count != 0);
            if (chosen == nullptr || behind == 1)
            {
                chosen = farthest;
            }
        }
        else
        {
            chosen = farthest;
        }
        if (chosen != nullptr)
        {
            output = marker_position(chosen);
            return true;
        }
    }
    const i32 floor = i32(player_record(context.actor_bytes).floor);
    const i32 cell = entity_detail::navigation_cell_index(floor, position);
    if (cell == -1 || (entity_detail::g_nav_grids[floor]->cells[size_t(cell)].flags_7 & 8) != 0)
    {
        const float increment = maximum * .5f;
        i32 index = 0;
        do
        {
            if (entity_detail::select_navigation_spawn_position(floor, nullptr, position, 0.f,
                float(index) * increment + maximum, nullptr, context.allowed, 0xE,
                i16(context.actor.class_key), output, context.random, context.occupancy_initial))
            {
                return true;
            }
            ++index;
        }
        while (index < 5);
        position = {player_record(context.actor_bytes).start_position};
        return entity_detail::select_navigation_spawn_position(floor, nullptr, position, 0.f,
            maximum, nullptr, context.allowed, 0xE, i16(context.actor.class_key), output,
            context.random, context.occupancy_initial);
    }
    RuntimeNavigationHeadingSearch search{position, floor, minimum, maximum + .5f,
        nullptr, context.allowed, &output, 1, 0xE, i16(context.actor.class_key), context.random,
        context.occupancy_initial};
    return search_runtime_navigation_headings(search, heading, std::bit_cast<float>(0x3E851EB8u), 12);
}
}
