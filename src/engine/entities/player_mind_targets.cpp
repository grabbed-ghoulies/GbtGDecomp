#include "player_mind_targets.h"
#include <bit>

namespace gbtg
{
// Xbox 0x00129920
bool runtime_mind_target_allowed(const RuntimeMindTarget& target, u32 flags)
{
    if (target.bytes[0x18] != 0
        || ((flags & 4) != 0 && (target.bytes[0x16] & 1) == 0)
        || ((flags & 8) == 0 && target.bytes[0x15] != 0))
    {
        return false;
    }
    if ((flags & 0x10) != 0 && target.bytes[0x14] == 0 && script_actor_is_active(target.actor))
    {
        return false;
    }
    if (((flags & 0x200) != 0 && (target.bytes[0x16] & 4) != 0)
        || ((flags & 0x400) != 0 && target.bytes[0x14] == 0 && read_u32(target.actor_bytes, 0x98C) != 0))
    {
        return false;
    }
    if ((flags & 0x800) != 0 && target.bytes[0x14] == 0 && script_mind_test_bit(target.body, 0x1B))
    {
        return false;
    }
    return true;
}

// Xbox 0x001299B0
bool runtime_mind_target_reachable(const Actor& source, const OccupancyRecord* allowed,
    const RuntimeMindTarget& target, u32 flags)
{
    if ((flags & 0x20) != 0)
    {
        Vec3 destination{read_f32(target.bytes, 0x20), read_f32(target.bytes, 0x24), read_f32(target.bytes, 0x28)};
        const Vec3 position = actor_world_position(source);
        const Vec3 delta{read_f32(target.bytes, 0x20) - position.x,
            read_f32(target.bytes, 0x24) - position.y, read_f32(target.bytes, 0x28) - position.z};
        float angle;
        runtime_direction_angle(delta, angle);
        angle = float(static_cast<long double>(angle) + std::bit_cast<float>(0x40490FDBu));
        const float radius = read_f32(target.bytes, 0x38);
        const float sine = float(retail_sin(angle));
        destination.x = sine * radius + destination.x;
        destination.z = float(retail_cos(angle) * static_cast<long double>(radius)
            + static_cast<long double>(destination.z));
        const Vec3 refreshed_position = actor_world_position(source);
        i32 last_cell;
        if (!entity_detail::navigation_path_clear(refreshed_position, destination, 0,
            (flags >> 6) & 1, (flags >> 8) & 1, 0, allowed, &last_cell, i16(source.class_key)))
        {
            return false;
        }
    }
    if ((flags & 0x80) != 0 && target.bytes[0x14] == 0 && target.occupancy != nullptr
        && target.occupancy != allowed
        && (source.class_key == 0 || target.occupancy->group == 0 || u16(target.occupancy->group) == source.class_key))
    {
        return false;
    }
    return true;
}

// Xbox 0x00129640
RuntimeMindTarget* find_runtime_mind_target(const Actor& source, const OccupancyRecord* allowed,
    u16 include_class, u16 exclude_class, float* distance, u32 category_mask,
    u32 predicate_flags, RuntimeMindTarget* first)
{
    RuntimeMindTarget* chosen = nullptr;
    const Vec3 position = actor_world_position(source);
    float best;
    for (RuntimeMindTarget* target = first; target != nullptr; target = target->next)
    {
        if ((include_class == 0 || target->actor.class_key == include_class)
            && (exclude_class == 0 || target->actor.class_key != exclude_class)
            && &target->actor != &source
            && (category_mask & (1u << (target->bytes[0x14] & 31))) != 0
            && runtime_mind_target_allowed(*target, predicate_flags))
        {
            const float x = position.x - read_f32(target->bytes, 0x20);
            const float y = position.y - read_f32(target->bytes, 0x24);
            const float z = position.z - read_f32(target->bytes, 0x28);
            const float squared = z * z + x * x + y * y;
            if ((chosen == nullptr || best > squared)
                && runtime_mind_target_reachable(source, allowed, *target, predicate_flags))
            {
                chosen = target;
                best = squared;
            }
        }
    }
    if (chosen != nullptr && distance != nullptr)
    {
        *distance = float(retail_square_root(best));
    }
    return chosen;
}
}
