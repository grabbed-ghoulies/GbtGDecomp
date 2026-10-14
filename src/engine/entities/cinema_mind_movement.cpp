#include "cinema_mind_movement.h"
#include "body_request_selection.h"
#include "player_mind_requests.h"
#include "actor_transform.h"
#include "body_damage_motion.h"
#include "shape_copy.h"
#include "actor_spawn_eligibility.h"
#include "actor_spawn_query.h"
#include "../navigation/floor_tracking.h"
#include "../audio/body_sound_commands.h"
#include "gbtg/anim.h"
#include "../navigation/mind_navigation_angles.h"
#include "../navigation/mind_navigation_costs.h"
#include <bit>
#include <cstring>
#include <xmmintrin.h>

namespace gbtg
{
namespace
{
Vec3 point(std::span<const u8> bytes, size_t offset)
{
    return {read_f32(bytes, offset), read_f32(bytes, offset + 4), read_f32(bytes, offset + 8)};
}
void value(std::span<u8> bytes, size_t offset, float number)
{
    std::memcpy(bytes.data() + offset, &number, 4);
}
}

// Xbox 0x0007BF60
u32 update_runtime_cinema_mind_spawn_position(RuntimeCinemaMindSpawnMovement& context)
{
    auto& movement = context.movement;
    if (read_u32(movement.mind, 0x18) == 0)
    {
        return 1;
    }
    auto* target = context.target;
    if (!actor_can_supply_ghoulybox(*target->actor))
    {
        const Vec3 position = actor_world_position(movement.actor);
        Actor* selected = find_nearest_spawn_actor(position, &movement.actor, nullptr, 0x1F);
        if (selected == nullptr)
        {
            const u32 zero = 0;
            std::memcpy(movement.mind.data() + 0x18, &zero, 4);
            context.target = nullptr;
            return 1;
        }
        target = &context.actor_at(*selected);
        context.target = target;
        std::memcpy(movement.mind.data() + 0x18, &target->address, 4);
    }
    if (read_u16(movement.body, 0x30) != 0 || read_f32(movement.body, 0x34) != 0.f)
    {
        return 0;
    }
    const Vec3 position = actor_world_position(movement.actor);
    Vec3 destination;
    float radius;
    i32 index;
    if (select_actor_spawn_point(*target->actor, position, destination, radius, index))
    {
        u32 failed;
        drive_runtime_player_navigation(movement.navigation, destination,
            i32(read_u32(target->bytes, 0xD00)), 0.f, 30.f, 1.f, 0, failed);
        if (runtime_player_can_attach(movement.eligibility, *target, context.target_link(*target))
            && read_u32(movement.mind, 0x90) != 0)
        {
            return 1;
        }
        if (failed == 0)
        {
            return 0;
        }
        const i32 cell = entity_detail::navigation_cell_index(i32(read_u32(movement.actor_bytes, 0xCEC)), position);
        if (cell != -1 && !entity_detail::navigation_cell_has_no_unblocked_neighbor(
            *movement.navigation.navigation.search->grid, u32(cell)))
        {
            return 0;
        }
    }
    return 4;
}

// Xbox 0x0007BA20
u32 update_runtime_cinema_mind_timed_motion(RuntimeCinemaMindMovement& context,
    std::span<const u8> request, const RuntimeBodyDamageTables& tables,
    std::span<const float> mode_scales)
{
    if (read_u16(context.body, 0x30) != 0 || read_f32(context.body, 0x34) != 0.f)
    {
        return 0;
    }
    const u8 mode = context.body[read_u16(context.body, 0x880) == 0 ? 0x874 : 0x87E];
    float multiplier = 1.f;
    if ((read_u32(context.body, 0x878) & (1u << (mode & 31))) != 0)
    {
        multiplier = mode_scales[mode]; // DAT_00217EC0.
    }
    const float speed = runtime_body_step_speed(context.body, context.eligibility.body_actor, tables)
        * multiplier * read_f32(request, 0x1C);
    const float angle = read_f32(request, 0x28);
    const float dx = float(retail_sin(angle) * speed);
    const float dz = float(retail_cos(angle) * speed);
    const Vec3 position = actor_world_position(context.actor);
    const float remaining = read_f32(request, 0x18) - read_f32(request, 0xC);
    const Vec3 destination{remaining * dx + position.x, remaining * 0.f + position.y,
        remaining * dz + position.z};
    std::array<float, 2> axes;
    bool write_axes = true;
    if (read_u32(request, 0x24) == 0)
    {
        u32 failed;
        const u32 result = drive_runtime_player_navigation(context.navigation, destination,
            i32(read_u32(context.actor_bytes, 0xCEC)), 0.f, 30.f, 1.f, 0, failed);
        if (result != 0)
        {
            write_axes = false;
        }
        else
        {
            runtime_input_axes(context.navigation.input, axes, context.navigation.input_state);
            scale_runtime_planar_direction(axes, read_f32(request, 0x1C));
        }
    }
    else
    {
        axes = {dz, dx};
        const float square = dz * dz + dx * dx;
        if (square != 0.f)
        {
            const long double scale = static_cast<long double>(read_f32(request, 0x1C)) / retail_square_root(square);
            axes[0] = float(static_cast<long double>(dz) * scale);
            axes[1] = float(scale * dx);
        }
    }
    if (write_axes)
    {
        set_runtime_input_axes(context.navigation.writable, axes, context.navigation.keyboard);
    }
    return read_f32(request, 0xC) >= read_f32(request, 0x18) ? 1 : 0;
}

// Xbox 0x0007B840
u32 request_runtime_cinema_mind_spawn_attachment(RuntimeCinemaMindMovement& context,
    RuntimeBodyAttachedActor* target,
    const std::function<std::span<const u8>(const RuntimeBodyAttachedActor&)>& target_link,
    const std::function<u16(RuntimeBodyAttachedActor&)>& attach,
    std::span<u8> body, RuntimeBodyActionSelection& actions)
{
    if (read_u32(context.mind, 0x18) != 0 && !(read_f32(context.mind, 0x98) > 0.f))
    {
        if (actor_can_supply_ghoulybox(*target->actor) && target->bytes[0x93C] != 0)
        {
            if (runtime_player_can_attach(context.eligibility, *target, target_link(*target)))
            {
                const u16 action_key = attach(*target); // Actual body vtable +0x5C, AX feeds 0x00027650.
                select_runtime_body_action(body, action_key, actions);
            }
        }
    }
    return 1;
}

// Xbox 0x0007B090
bool update_runtime_cinema_mind_target_action(RuntimeCinemaMindMovement& context,
    std::span<const u8> request, const std::function<RuntimeMindChainRequestSelection&()>& selection,
    std::span<u8> input, const RuntimeInputReleaseContext& input_context)
{
    if (read_u32(context.mind, 0x10) == 0)
    {
        return true;
    }
    if (read_u16(context.body, 0x30) == 0 && read_f32(context.body, 0x34) == 0.f)
    {
        if (read_u32(request, 0x14) != 0 || context.target->bytes[0x15] == 0)
        {
            return dispatch_runtime_mind_target_request(selection(), 0xFFFFFFFFu, read_u32(request, 0x10)) != 1;
        }
        if (read_u32(context.actor_bytes, 0x960) != 0)
        {
            reset_runtime_input_device(input, input_context);
        }
    }
    return false;
}

// Xbox 0x0007BC80
u32 update_runtime_cinema_mind_target_position(RuntimeCinemaMindMovement& context, std::span<const u8> request)
{
    if (read_u32(context.mind, 0x10) == 0)
    {
        return 1;
    }
    auto& target = *context.target;
    if (read_u32(request, 0x28) != 0 && !runtime_mind_target_is_movable(target))
    {
        return 0;
    }
    const i32 floor = target.bytes[0x19];
    const Vec3 target_position = point(target.bytes, 0x20);
    const i32 target_cell = entity_detail::navigation_cell_index(floor, target_position);
    bool isolated;
    std::array<u8, 0x20> shape;
    if (target_cell == -1)
    {
        isolated = true;
        copy_actor_shape(shape.data(), target.bytes.data() + 0x3C);
    }
    else
    {
        isolated = entity_detail::navigation_cell_has_no_open_neighbor(*entity_detail::g_nav_grids[floor], u32(target_cell), 0);
        copy_actor_shape(shape.data(), target.bytes.data() + (isolated ? 0x3C : 0x1C));
        if (!isolated && (target.bytes[0x16] & 0x10) != 0)
        {
            Vec3 candidate;
            offset_runtime_horizontal_point(candidate, point(shape, 4), target.actor.euler.y, read_f32(shape, 0x1C) * 2.f);
            const i32 cell = entity_detail::navigation_cell_index(floor, candidate);
            if (cell != -1 && !entity_detail::navigation_cell_has_no_open_neighbor(*entity_detail::g_nav_grids[floor], u32(cell), 0))
            {
                std::memcpy(shape.data() + 4, &candidate, sizeof(candidate));
            }
        }
    }
    Vec3 destination = point(shape, 4);
    if (read_f32(request, 0x20) > 0.f)
    {
        const Vec3 position = actor_world_position(context.actor);
        const float dx = destination.x - position.x;
        const float dy = destination.y - position.y;
        const float dz = destination.z - position.z;
        if ((read_f32(target.bytes, 0x64) * dz + dx * read_f32(target.bytes, 0x5C)) + dy * 0.f < 0.f)
        {
            const float offset = read_f32(request, 0x20);
            destination.x = read_f32(target.bytes, 0x5C) * offset + destination.x;
            destination.y = offset * 0.f + destination.y;
            destination.z = read_f32(target.bytes, 0x64) * offset + destination.z;
        }
    }
    const Vec3 position = actor_world_position(context.actor);
    i32 cell;
    float radius = 0.f;
    if (entity_detail::navigation_path_clear(position, destination, 1, 0, 0, 0, nullptr, &cell, i16(context.actor.class_key)))
    {
        const float body_radius = context.eligibility.body_actor.radius_xz;
        radius = runtime_player_body_attachment_radius(context.eligibility)
            + read_f32(context.body, 0x888) * body_radius;
        radius = radius + read_f32(request, 0x18);
        radius = radius + read_f32(shape, 0x1C);
        radius = radius + 1.f;
    }
    u32 failed;
    const u32 result = drive_runtime_player_navigation(context.navigation, destination, floor, radius,
        read_f32(request, 0x1C), read_f32(request, 0x2C), isolated ? 0 : (target.bytes[0x16] & 0x10) >> 4, failed);
    if (read_u32(request, 0x24) != 0 && result == 1)
    {
        if (!script_mind_is_idle(context.body_query, 1))
        {
            return 0;
        }
    }
    return result;
}

// Xbox 0x0007B620
u32 update_runtime_cinema_mind_vertical_target(RuntimeCinemaMindMovement& context, std::span<const u8> request)
{
    if (read_u32(context.mind, 0x10) == 0)
    {
        return 1;
    }
    auto& target = *context.target;
    if (read_u32(request, 0x34) != 0 && !runtime_mind_target_is_movable(target))
    {
        return 0;
    }
    Vec3 destination = point(target.bytes, 0x20);
    if (read_f32(request, 0x2C) > 0.f)
    {
        const Vec3 position = actor_world_position(context.actor);
        const float dx = position.x - read_f32(target.bytes, 0x20);
        const float dy = position.y - read_f32(target.bytes, 0x24);
        const float dz = position.z - read_f32(target.bytes, 0x28);
        if ((read_f32(target.bytes, 0x64) * dz + dx * read_f32(target.bytes, 0x5C)) + dy * 0.f < 0.f)
        {
            const float offset = read_f32(request, 0x2C);
            destination.x = read_f32(target.bytes, 0x5C) * offset + destination.x;
            destination.y = offset * 0.f + destination.y;
            destination.z = read_f32(target.bytes, 0x64) * offset + destination.z;
        }
    }
    const Vec3 position = actor_world_position(context.actor);
    i32 cell;
    float radius;
    if (!entity_detail::navigation_path_clear(position, destination, 1, 0, 0, 0,
        nullptr, &cell, i16(context.actor.class_key)))
    {
        radius = 0.f;
    }
    else
    {
        radius = runtime_player_target_radius(context.body, context.eligibility.body_actor,
            context.eligibility.attached, target.actor) + read_f32(request, 0x20) + 1.f;
    }
    u32 failed;
    const u32 result = drive_runtime_mind_vertical_navigation(context.navigation, destination, target.bytes[0x19],
        read_f32(request, 0x18), read_f32(request, 0x1C), radius, read_f32(request, 0x24),
        read_f32(request, 0x38), read_f32(request, 0x28), (target.bytes[0x16] & 0x10) >> 4, failed);
    if (read_u32(request, 0x30) != 0 && result == 1)
    {
        if (!script_mind_is_idle(context.body_query, 1))
        {
            return 0;
        }
    }
    return result;
}

// Xbox 0x0007B170
u32 update_runtime_cinema_mind_escape_motion(RuntimeCinemaMindMovement& context, std::span<u8> request)
{
    if (read_u32(context.actor_bytes, 0x958) == 0 || context.escape.target.data() == nullptr)
    {
        return 1;
    }
    const float distance = runtime_body_target_surface_distance(context.actor, context.eligibility, context.escape.target);
    if (distance > read_f32(request, 0x28))
    {
        return 1;
    }
    const float step = runtime_frame_step(context.clock);
    const float rate = runtime_actor_rate(context.actor);
    const float timer = read_f32(request, 0x30) - rate * step;
    value(request, 0x30, timer);
    if (timer <= 0.f)
    {
        value(request, 0x30, timer + 2.f);
        Vec3 selected;
        if (!select_runtime_mind_escape_position(context.escape, read_f32(request, 0x2C),
            read_f32(request, 0x10), read_f32(request, 0x14), selected))
        {
            value(request, 0x30, 0.f);
            return 0;
        }
        std::memcpy(context.mind.data() + 0x70, &selected, sizeof(selected));
    }
    const Vec3 destination = point(context.mind, 0x70);
    float heading;
    randomize_runtime_mind_escape_heading(context.actor, context.escape.target, destination, heading, context.random);
    value(request, 0x2C, heading);
    auto* costs = suspend_runtime_mind_target_costs(context.actor_bytes, context.body,
        context.body_actor_bytes, context.escape.target);
    u32 failed;
    const u32 result = drive_runtime_player_navigation(context.navigation, destination, -1,
        2.5f, 0.f, read_f32(request, 0x20), 0, failed);
    if (costs != nullptr)
    {
        restore_runtime_mind_target_costs(context.actor_bytes, costs);
    }
    if (result == 1)
    {
        value(request, 0x30, 0.f);
    }
    return 0;
}

// Xbox 0x0007B2F0
u32 update_runtime_cinema_mind_vertical_escape_motion(RuntimeCinemaMindMovement& context, std::span<u8> request)
{
    if (read_u32(context.actor_bytes, 0x958) == 0 || context.escape.target.data() == nullptr)
    {
        return 1;
    }
    const float distance = runtime_body_target_surface_distance(context.actor, context.eligibility, context.escape.target);
    if (distance > read_f32(request, 0x34))
    {
        return 1;
    }
    const float step = runtime_frame_step(context.clock);
    const float rate = runtime_actor_rate(context.actor);
    const float timer = read_f32(request, 0x3C) - rate * step;
    value(request, 0x3C, timer);
    if (timer <= 0.f)
    {
        value(request, 0x3C, timer + 2.f);
        Vec3 selected;
        if (!select_runtime_mind_escape_position(context.escape, read_f32(request, 0x38),
            read_f32(request, 0x18), read_f32(request, 0x1C), selected))
        {
            value(request, 0x3C, 0.f);
            return 0;
        }
        std::memcpy(context.mind.data() + 0x70, &selected, sizeof(selected));
    }
    const Vec3 destination = point(context.mind, 0x70);
    float heading;
    randomize_runtime_mind_escape_heading(context.actor, context.escape.target, destination, heading, context.random);
    value(request, 0x38, heading);
    auto* costs = suspend_runtime_mind_target_costs(context.actor_bytes, context.body,
        context.body_actor_bytes, context.escape.target);
    const i32 floor = entity_detail::navigation_floor_index(destination, i32(read_u32(context.actor_bytes, 0xCEC)), 0);
    u32 failed;
    const u32 result = drive_runtime_mind_vertical_navigation(context.navigation, destination, floor,
        read_f32(request, 0x10), read_f32(request, 0x14), 2.5f, 0.f,
        read_f32(request, 0x28), read_f32(request, 0x2C), 0, failed);
    if (costs != nullptr)
    {
        restore_runtime_mind_target_costs(context.actor_bytes, costs);
    }
    if (result == 1)
    {
        value(request, 0x3C, 0.f);
    }
    return 0;
}

// Xbox 0x0007B5A0
u32 update_runtime_cinema_mind_vertical_position(RuntimeCinemaMindMovement& context, std::span<const u8> request)
{
    const Vec3 destination = point(context.mind, 0x70);
    const i32 floor = entity_detail::navigation_floor_index(destination, i32(read_u32(context.actor_bytes, 0xCEC)), 0);
    u32 failed;
    const u32 result = drive_runtime_mind_vertical_navigation(context.navigation, destination, floor,
        read_f32(request, 0x18), read_f32(request, 0x1C), read_f32(request, 0x20) + 1.f,
        read_f32(request, 0x24), read_f32(request, 0x2C), read_f32(request, 0x28), 0, failed);
    if (result == 0 && runtime_player_navigation_fallback(context.navigation, destination))
    {
        return 1;
    }
    return result;
}

// Xbox 0x00136680
u32 update_runtime_mind_vertical_position_request(RuntimeCinemaMindMovement& context, std::span<const u8> request)
{
    const Vec3 destination = point(request, 0x10);
    const i32 floor = entity_detail::navigation_floor_index(destination, i32(read_u32(context.actor_bytes, 0xCEC)), 0);
    u32 failed;
    const u32 result = drive_runtime_mind_vertical_navigation(context.navigation, destination, floor,
        read_f32(request, 0x24), read_f32(request, 0x28), read_f32(request, 0x2C) + 2.5f,
        read_f32(request, 0x30), read_f32(request, 0x38), read_f32(request, 0x34), 0, failed);
    if (result == 0 && runtime_player_navigation_fallback(context.navigation, point(context.mind, 0x70)))
    {
        return 1;
    }
    return result;
}

// Xbox 0x0007BC20
u32 update_runtime_cinema_mind_position(RuntimeCinemaMindMovement& context, std::span<const u8> request)
{
    const Vec3 destination = point(context.mind, 0x70);
    u32 failed;
    const u32 result = drive_runtime_player_navigation(context.navigation, destination, -1,
        float(static_cast<long double>(read_f32(request, 0x18)) + 1.L),
        read_f32(request, 0x1C), read_f32(request, 0x20), 0, failed);
    if (result == 0 && runtime_player_navigation_fallback(context.navigation, destination))
    {
        return 1;
    }
    return result;
}

// Xbox 0x0007B8A0
u32 select_runtime_cinema_mind_random_position(RuntimeCinemaMindMovement& context, std::span<const u8> request)
{
    const Vec3 position = actor_world_position(context.actor);
    Vec3 selected;
    if (entity_detail::select_navigation_spawn_position(i32(read_u32(context.actor_bytes, 0xCEC)), nullptr,
        position, read_f32(request, 0x10), read_f32(request, 0x14), context.required, context.allowed,
        0x1C, i16(context.actor.class_key), selected, context.random, context.occupancy_initial))
    {
        std::memcpy(context.mind.data() + 0x70, &selected, sizeof(selected));
        return 1;
    }
    return 4;
}

// Xbox 0x0007B940
u32 select_runtime_cinema_mind_heading_position(RuntimeCinemaMindMovement& context, std::span<const u8> request)
{
    const Vec3 position = actor_world_position(context.actor);
    const float heading = randomize_runtime_navigation_heading(context.actor.euler.y, read_f32(request, 0x10), context.random);
    Vec3 selected;
    RuntimeNavigationHeadingSearch search{position, i32(read_u32(context.actor_bytes, 0xCEC)),
        read_f32(request, 0x14), float(static_cast<long double>(read_f32(request, 0x18)) + .5L),
        context.required, context.allowed, &selected, 1, 0x1C, i16(context.actor.class_key), context.random,
        context.occupancy_initial};
    const float increment = read_f32(request, 0x1C);
    const i32 attempts = _mm_cvttss_si32(_mm_set_ss(std::bit_cast<float>(0x40490FDBu) / increment));
    if (search_runtime_navigation_headings(search, heading, increment, attempts))
    {
        std::memcpy(context.mind.data() + 0x70, &selected, sizeof(selected));
        return 1;
    }
    return 4;
}
}
