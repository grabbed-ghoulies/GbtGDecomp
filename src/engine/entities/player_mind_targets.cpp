#include "player_mind_targets.h"
#include "mind_fields.h"
#include "actor_transform.h"
#include "body_damage_motion.h"
#include "body_gaze.h"
#include "body_message_helpers.h"
#include "body_fields.h"
#include "player_fields.h"
#include <bit>
#include <cstring>
#include <cmath>

namespace gbtg
{
// Xbox 0x00134650
bool runtime_body_target_state_active(std::span<const u8> body)
{
    if (read_f32(body, 0x23C) != 0.f
        && (get(body, body_field::energy) <= 1 || read_f32(body, 0x174) > 0.f
            || read_f32(body, 0x16C) == 0.f))
    {
        return true;
    }
    return false;
}

// Xbox 0x00134620, ECX body
bool runtime_body_target_state_flagged(std::span<const u8> body, const EntityBitBlock& bits)
{
    if ((bits.bytes[3] & 2) != 0)
    {
        return true;
    }
    if ((bits.bytes[2] & 2) != 0)
    {
        return runtime_body_target_state_active(body); // The tail jump to 0x00134650.
    }
    return false;
}

// Xbox 0x001413C0
bool runtime_mind_target_requires_action(const RuntimeMindTarget& target, std::span<const u8> body)
{
    if ((get(target.bytes, mind_target_field::flags) & 8) != 0)
    {
        return true;
    }
    switch (get(target.bytes, mind_target_field::kind))
    {
    case 0:
    {
        const auto& flags = *target.body.bits;
        constexpr std::array<u32, 5> indices{0x10, 0x24, 0x25, 0x27, 0x34};
        for (u32 index : indices)
        {
            if ((flags.bytes[index >> 3] & (1u << (index & 7))) != 0)
            {
                return true;
            }
        }
        if ((flags.bytes[3] & 2) != 0)
        {
            return true;
        }
        if ((flags.bytes[2] & 2) == 0)
        {
            return false;
        }
        return runtime_body_target_state_active(body);
    }
    case 1:
    case 2:
        return true;
    default:
        return false;
    }
}

// Xbox 0x0001D130
bool runtime_mind_target_requires_special_action(const RuntimeMindTarget& target,
    std::span<const u8> body, std::span<const u8> actor, const u32& first_mode, const u32& second_mode)
{
    if (runtime_mind_target_requires_action(target, body))
    {
        return true;
    }
    if (get(actor, player_field::live) == 0)
    {
        return false;
    }
    if (get(actor, player_field::mode) != 2 && get(actor, player_field::case_link) == 0)
    {
        return false;
    }
    return first_mode != 0 || second_mode != 0;
}

// Xbox 0x0007D770
u32 runtime_player_target_search_mode(const RuntimePlayerAttachmentEligibility& body,
    std::span<const u8> policy)
{
    const auto* attached = body.attached;
    if (attached != nullptr)
    {
        const u8 kind = attached->bytes[0x93D];
        if (kind >= 2)
        {
            if (kind <= 3)
            {
                return 2;
            }
            if (kind == 4 && (read_u32(policy, 0xB0) == 3 || (body.flags.bytes[5] & 0x80) != 0))
            {
                return 3;
            }
        }
    }
    return 1;
}

// Xbox 0x0007D7C0
bool runtime_player_keeps_held_target(const RuntimeMindTarget* target,
    const RuntimePlayerAttachmentEligibility& body, const Actor& origin,
    float heading, float* angle, float angular_limit)
{
    if (target != nullptr && target->bytes[0x14] == 0 && target->body.energy != 0
        && body.attached != nullptr && body.attached->bytes[0x93D] == 4)
    {
        if (runtime_body_bit_set(body.flags, 0x2F))
        {
            const float target_angle = runtime_actor_bound_angle(target->bytes, origin);
            const float relative = runtime_relative_angle(heading, target_angle);
            if (!(animation_absolute(relative) > static_cast<long double>(angular_limit)))
            {
                if (angle != nullptr)
                {
                    *angle = target_angle;
                }
                return true;
            }
        }
    }
    return false;
}

// Xbox 0x00137DE0
RuntimeMindTarget* search_runtime_player_target_windows(RuntimePlayerTargetSearch& context,
    float heading, float scale, u32 category_mask, u32 predicate_flags, u32 mode, float* angle)
{
    constexpr std::array<u32, 18> windows{
        0x42C80000, 0x41900000, 0x42680000, 0x41E00000, 0x41D00000, 0x422C0000,
        0x41700000, 0x42960000, 0x42200000, 0x42160000, 0, 0,
        0x41C80000, 0x42F00000, 0x42200000, 0x42A00000, 0x42700000, 0x42200000};
    for (u32 index = 0; index < 3; ++index)
    {
        float degrees;
        float radius;
        if (mode >= 1 && mode <= 3)
        {
            const size_t offset = (mode - 1) * 6 + index * 2;
            degrees = std::bit_cast<float>(windows[offset]);
            radius = std::bit_cast<float>(windows[offset + 1]);
        }
        else
        {
            degrees = read_f32(context.address_zero, 0);
            radius = read_f32(context.address_zero, 4);
        }
        if (degrees == 0.f)
        {
            return nullptr;
        }
        u16 excluded = 0;
        if (get(context.actor_bytes, player_field::countdown_56) > 0.f ||
            get(context.actor_bytes, player_field::provoked) != 0)
        {
            const u16 value = read_u16(context.actor_bytes, 0x97E);
            if (value > 1)
            {
                excluded = value;
            }
        }
        u16 included = 0;
        if (get(context.actor_bytes, player_field::countdown_56) > 0.f ||
            get(context.actor_bytes, player_field::provoked) != 0)
        {
            const u16 value = read_u16(context.actor_bytes, 0x97C);
            if (value > 1)
            {
                included = value;
            }
        }
        const float half_angle = degrees * std::bit_cast<float>(0x3C8EFA35u);
        float distance;
        RuntimeMindTarget* target = find_runtime_mind_target_in_angle(context.actor, nullptr,
            heading, half_angle, included, excluded, &context.retained_angle, &distance,
            category_mask, predicate_flags, radius * scale, context.first);
        if (angle != nullptr)
        {
            *angle = context.retained_angle;
        }
        if (target != nullptr)
        {
            return target;
        }
    }
    return nullptr;
}

// Xbox 0x00137D00
RuntimeMindTarget* search_runtime_player_target_categories(RuntimePlayerTargetSearch& context,
    float heading, float* angle, u32 category_mask, u32 predicate_flags, u32 mode)
{
    RuntimeMindTarget* fallback = nullptr;
    const auto* attached = context.body.attached;
    const float scale = attached == nullptr || attached->bytes[0x93D] < 2
        || attached->bytes[0x93D] > 3 ? 1.f : 3.f;
    for (u32 index = 0; index < 4; ++index)
    {
        const u32 category = 1u << index;
        if ((category_mask & category) != 0)
        {
            RuntimeMindTarget* target = search_runtime_player_target_windows(context, heading,
                scale, category, predicate_flags, mode, angle);
            if (target != nullptr)
            {
                attached = context.body.attached;
                if (attached != nullptr && attached->bytes[0x93D] != 0)
                {
                    return target;
                }
                if (runtime_player_body_point_is_near(context.body, target->bytes))
                {
                    return target;
                }
                if (fallback == nullptr)
                {
                    fallback = target;
                }
            }
        }
    }
    return fallback;
}

// Xbox 0x0007D6B0
RuntimeMindTarget* select_runtime_player_shape_target(RuntimePlayerTargetSearch& context,
    std::span<const u8> policy, float* angle, RuntimeMindTarget* current, float heading)
{
    const u32 mode = runtime_player_target_search_mode(context.body, policy);
    u32 categories = read_u32(policy, 0x68);
    u32 predicate = read_u32(policy, 0x64);
    const auto* attached = context.body.attached;
    const u8 kind = attached == nullptr ? 0 : attached->bytes[0x93D];
    switch (kind)
    {
    case 2:
        categories &= ~4u;
        break;
    case 3:
        categories &= ~6u;
        break;
    case 4:
        if (runtime_player_keeps_held_target(current, context.body, context.actor,
            heading, angle, context.angular_limit))
        {
            return current;
        }
        if (read_u32(policy, 0xB0) == 3)
        {
            categories &= ~6u;
            predicate |= 0x800;
        }
        break;
    default:
        break;
    }
    return search_runtime_player_target_categories(context, heading, angle, categories,
        predicate, mode);
}

// Xbox 0x00129B20
void score_runtime_mind_target(const Vec3& origin, const Vec3& position,
    float heading, float half_angle, float radius, RuntimeMindTargetScore& score)
{
    const long double dx = static_cast<long double>(origin.x) - position.x;
    const long double dy = static_cast<long double>(origin.y) - position.y;
    const long double dz = static_cast<long double>(origin.z) - position.z;
    score.distance = float(std::sqrt((dy * dy + dx * dx) + dz * dz));
    const Vec3 direction{position.x - origin.x, position.y - origin.y,
        position.z - origin.z};
    runtime_direction_angle(direction, score.angle);
    const float turn = std::bit_cast<float>(0x40C90FDBu);
    const long double remainder = animation_remainder(score.angle - heading, turn);
    float relative = float(remainder);
    if (static_cast<long double>(std::bit_cast<float>(0xC0490FDBu)) > remainder)
    {
        relative += turn;
    }
    if (relative >= std::bit_cast<float>(0x40490FDBu))
    {
        relative -= turn;
    }
    const long double magnitude = animation_absolute(relative);
    score.relative_angle = float(magnitude);
    score.score = float(((1.L - static_cast<long double>(score.distance) / radius)
        + (1.L - magnitude / half_angle)) * .5L);
}

// Xbox 0x00129770
RuntimeMindTarget* find_runtime_mind_target_in_angle(const Actor& source,
    const OccupancyRecord* allowed, float heading, float half_angle,
    u16 include_class, u16 exclude_class, float* angle, float* distance,
    u32 category_mask, u32 predicate_flags, float radius, RuntimeMindTarget* first)
{
    if (!(radius > 0.f))
    {
        radius = std::bit_cast<float>(0x47C35000u);
    }
    RuntimeMindTarget* chosen = nullptr;
    float chosen_distance = radius;
    float chosen_angle = heading + std::bit_cast<float>(0x40490FDBu);
    float best_score = 0.f;
    const Vec3 origin = actor_world_position(source);
    for (RuntimeMindTarget* target = first; target != nullptr; target = target->next)
    {
        if ((include_class == 0 || target->actor.class_key == include_class)
            && (exclude_class == 0 || target->actor.class_key != exclude_class)
            && &target->actor != &source
            && (category_mask & (1u << (target->bytes[0x14] & 31))) != 0
            && runtime_mind_target_allowed(*target, predicate_flags))
        {
            const float reach = get(target->bytes, mind_target_field::shape_radius) + radius;
            const Vec3 position{get(target->bytes, mind_target_field::shape_position).x,
                get(target->bytes, mind_target_field::shape_position).y, get(target->bytes, mind_target_field::shape_position).z};
            RuntimeMindTargetScore score;
            score_runtime_mind_target(origin, position, heading, half_angle, reach, score);
            if (reach > score.distance && half_angle > score.relative_angle
                && score.score > best_score
                && runtime_mind_target_reachable(source, allowed, *target, predicate_flags))
            {
                chosen = target;
                chosen_distance = score.distance;
                chosen_angle = score.angle;
                best_score = score.score;
            }
        }
    }
    if (chosen != nullptr)
    {
        if (angle != nullptr)
        {
            *angle = chosen_angle;
        }
        if (distance != nullptr)
        {
            *distance = chosen_distance;
        }
    }
    return chosen;
}

// Xbox 0x0007E6B0
Actor* update_runtime_player_mind_focus(RuntimePlayerMindFocus& context)
{
    Actor* target = context.target;
    Actor* const previous = target;
    const u32 previous_mode = get(context.mind, mind_field::target_mode);
    u32 mode = previous_mode;
    bool restore_previous;
    bool search_near;
    float distance;
    if (target != nullptr && (context.bodies.at(target)->flags.bytes[3] & 8) != 0)
    {
        mode = 0;
        restore_previous = true;
        search_near = true;
    }
    else
    {
        restore_previous = false;
        search_near = mode == 0 || target == nullptr;
        if (!search_near)
        {
            distance = float(runtime_actor_distance(context.actor, *target));
            search_near = distance > 20.f
                || (context.bodies.at(target)->flags.bytes[7] & 0x10) != 0;
        }
    }
    if (search_near)
    {
        target = select_runtime_player_near_actor(context.actor, distance,
            context.retained_cone_distance, context.query);
    }
    bool search_far;
    if (target == nullptr)
    {
        mode = 0;
        search_far = true;
    }
    else
    {
        const auto& body = *context.bodies.at(target);
        const float own_reach = context.body.body_actor.radius_xz
            * read_f32(context.body.body, 0x888);
        const float target_radius = body.body_actor.radius_xz;
        const float margin = runtime_player_body_attachment_radius(body);
        const float gap = distance - ((margin + read_f32(body.body, 0x888) * target_radius)
            + own_reach);
        if (60.f > gap)
        {
            mode = 1;
            search_far = false;
        }
        else if (gap > 70.f)
        {
            mode = 0;
            search_far = true;
        }
        else
        {
            search_far = mode == 0;
        }
    }
    if (search_far)
    {
        float far_distance;
        target = find_nearest_runtime_script_actor(nullptr, context.actor, 0, 200.f,
            &far_distance, 0x124344, nullptr, context.query);
        if (target != nullptr)
        {
            mode = 1;
        }
    }
    if (restore_previous && target == nullptr && previous != nullptr)
    {
        mode = previous_mode;
        target = previous;
    }
    if (target != nullptr)
    {
        const auto* body = context.bodies.at(target);
        if (body != nullptr && (body->flags.bytes[7] & 0x10) != 0)
        {
            mode = 0;
        }
    }
    context.target = target;
    const u32 address = target == nullptr ? 0 : context.addresses.at(target);
    set(context.mind, mind_field::target, address);
    if (mode != get(context.mind, mind_field::target_mode)
        && get(context.mind, mind_field::unk_38) > get(context.mind, mind_field::unk_34))
    {
        set(context.mind, mind_field::unk_34, get(context.mind, mind_field::unk_38));
    }
    set(context.mind, mind_field::target_mode, u8(mode));
    return target;
}

// Xbox 0x0007E600
Actor* select_runtime_player_near_actor(Actor& requester, float& distance,
    float& retained_cone_distance, const RuntimeScriptActorQueryContext& context)
{
    float near_distance = requester.euler.y;
    Actor* cone = find_nearest_runtime_script_actor_in_angle(requester, requester.euler.y,
        std::bit_cast<float>(0x3F490FDBu), 70.f, nullptr, &retained_cone_distance,
        0x24345, nullptr, context);
    Actor* nearest = find_nearest_runtime_script_actor(nullptr, requester, 0, 70.f,
        &near_distance, 0x24345, nullptr, context);
    if (nearest != cone)
    {
        if (nearest == nullptr)
        {
            if (cone == nullptr)
            {
                return nullptr;
            }
        }
        else
        {
            if (cone == nullptr)
            {
                distance = near_distance;
                return nearest;
            }
            if (!(near_distance > retained_cone_distance - 10.f))
            {
                distance = near_distance;
                return nearest;
            }
        }
    }
    distance = retained_cone_distance;
    return cone;
}

// Xbox 0x001414C0
bool runtime_mind_target_is_controlled_player(const RuntimeMindTarget& target)
{
    if (get(target.bytes, mind_target_field::kind) != 0)
    {
        return false;
    }
    if (get(target.actor_bytes, player_field::mode) == 2)
    {
        return true;
    }
    return get(target.actor_bytes, player_field::case_link) != 0;
}

// Xbox 0x0013A2A0
bool runtime_player_mind_saved_path_clear(const Actor& actor, std::span<const u8> mind,
    const OccupancyRecord* occupancy, const OccupancyRecord* allowed, u16 owner_class)
{
    const Vec3 point(get(mind, mind_field::goal_point));
    if (occupancy == nullptr)
    {
        const Vec3 position = actor_world_position(actor);
        i32 last_cell;
        if (entity_detail::navigation_path_clear(position, point, 0, 0, 1, 0,
                allowed, &last_cell, i16(owner_class)))
        {
            return true;
        }
    }
    return false;
}

// Xbox 0x00141760
bool runtime_mind_target_occupancy_differs(const RuntimeMindTarget& target,
    u16 owner_class, const OccupancyRecord* allowed)
{
    if (get(target.bytes, mind_target_field::kind) == 0 && target.occupancy != nullptr
        && target.occupancy != allowed
        && (owner_class == 0 || target.occupancy->group == 0
            || u16(target.occupancy->group) == owner_class))
    {
        return true;
    }
    return false;
}

// Xbox 0x00129920
bool runtime_mind_target_allowed(const RuntimeMindTarget& target, u32 flags)
{
    if (get(target.bytes, mind_target_field::unk_18) != 0
        || ((flags & 4) != 0 && (get(target.bytes, mind_target_field::flags) & 1) == 0)
        || ((flags & 8) == 0 && get(target.bytes, mind_target_field::hold) != 0))
    {
        return false;
    }
    if ((flags & 0x10) != 0 && get(target.bytes, mind_target_field::kind) == 0 && script_actor_is_active(target.actor))
    {
        return false;
    }
    if (((flags & 0x200) != 0 && (get(target.bytes, mind_target_field::flags) & 4) != 0) ||
        ((flags & 0x400) != 0 && get(target.bytes, mind_target_field::kind) == 0 &&
         get(target.actor_bytes, player_field::goal_volume) != 0))
    {
        return false;
    }
    if ((flags & 0x800) != 0 && get(target.bytes, mind_target_field::kind) == 0 && script_mind_test_bit(target.body, 0x1B))
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
        Vec3 destination{get(target.bytes, mind_target_field::shape_position).x, get(target.bytes, mind_target_field::shape_position).y, get(target.bytes, mind_target_field::shape_position).z};
        const Vec3 position = actor_world_position(source);
        const Vec3 delta{get(target.bytes, mind_target_field::shape_position).x - position.x,
            get(target.bytes, mind_target_field::shape_position).y - position.y, get(target.bytes, mind_target_field::shape_position).z - position.z};
        float angle;
        runtime_direction_angle(delta, angle);
        angle = float(static_cast<long double>(angle) + std::bit_cast<float>(0x40490FDBu));
        const float radius = get(target.bytes, mind_target_field::shape_radius);
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
    if ((flags & 0x80) != 0 && get(target.bytes, mind_target_field::kind) == 0 && target.occupancy != nullptr
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
            const float x = position.x - get(target->bytes, mind_target_field::shape_position).x;
            const float y = position.y - get(target->bytes, mind_target_field::shape_position).y;
            const float z = position.z - get(target->bytes, mind_target_field::shape_position).z;
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
