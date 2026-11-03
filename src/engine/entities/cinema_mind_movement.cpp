#include "cinema_mind_movement.h"
#include "player_mind_targets.h"
#include "mind_fields.h"
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
#include "body_fields.h"
#include "player_fields.h"
#include "mind_request_fields.h"
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
}

// Xbox 0x0007BF60
u32 update_runtime_cinema_mind_spawn_position(RuntimeCinemaMindSpawnMovement& context)
{
    auto& movement = context.movement;
    if (get(movement.mind, mind_field::action_actor) == 0)
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
            set(movement.mind, mind_field::action_actor, zero);
            context.target = nullptr;
            return 1;
        }
        target = &context.actor_at(*selected);
        context.target = target;
        set(movement.mind, mind_field::action_actor, target->address);
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
        if (runtime_player_can_attach(movement.eligibility, *target)
            && read_u32(movement.mind, 0x90) != 0)
        {
            return 1;
        }
        if (failed == 0)
        {
            return 0;
        }
        const i32 cell =
            entity_detail::navigation_cell_index(i32(get(movement.actor_bytes, player_field::floor)), position);
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
    if (get(context.body, body_field::move_holds) != 0 || get(context.body, body_field::move_hold_time) != 0.f)
    {
        return 0;
    }
    const u8 mode = context.body[get(context.body, body_field::surface_override_frames) == 0 ? 0x874 : 0x87E];
    float multiplier = 1.f;
    if ((get(context.body, body_field::scaled_surfaces) & (1u << (mode & 31))) != 0)
    {
        multiplier = mode_scales[mode]; // DAT_00217EC0.
    }
    const float speed = runtime_body_step_speed(context.body, context.eligibility.body_actor, tables)
        * multiplier * get(request, timed_motion_request::speed);
    const float angle = get(request, timed_motion_request::heading);
    const float dx = float(retail_sin(angle) * speed);
    const float dz = float(retail_cos(angle) * speed);
    const Vec3 position = actor_world_position(context.actor);
    const float remaining = get(request, timed_motion_request::duration) - get(request, mind_request_field::elapsed);
    const Vec3 destination{remaining * dx + position.x, remaining * 0.f + position.y,
        remaining * dz + position.z};
    std::array<float, 2> axes;
    bool write_axes = true;
    if (get(request, timed_motion_request::direct) == 0)
    {
        u32 failed;
        const u32 result = drive_runtime_player_navigation(context.navigation, destination,
            i32(get(context.actor_bytes, player_field::floor)), 0.f, 30.f, 1.f, 0, failed);
        if (result != 0)
        {
            write_axes = false;
        }
        else
        {
            runtime_input_axes(context.navigation.input, axes, context.navigation.input_state);
            scale_runtime_planar_direction(axes, get(request, timed_motion_request::speed));
        }
    }
    else
    {
        axes = {dz, dx};
        const float square = dz * dz + dx * dx;
        if (square != 0.f)
        {
            const long double scale =
                static_cast<long double>(get(request, timed_motion_request::speed)) / retail_square_root(square);
            axes[0] = float(static_cast<long double>(dz) * scale);
            axes[1] = float(scale * dx);
        }
    }
    if (write_axes)
    {
        set_runtime_input_axes(context.navigation.writable, axes, context.navigation.keyboard);
    }
    return get(request, mind_request_field::elapsed) >= get(request, timed_motion_request::duration) ? 1 : 0;
}

// Xbox 0x0007B840
u32 request_runtime_cinema_mind_spawn_attachment(RuntimeCinemaMindMovement& context,
    RuntimeBodyAttachedActor* target,
    const std::function<u16(RuntimeBodyAttachedActor&)>& attach,
    std::span<u8> body, RuntimeBodyActionSelection& actions)
{
    if (get(context.mind, mind_field::action_actor) != 0 && !(read_f32(context.mind, 0x98) > 0.f))
    {
        if (actor_can_supply_ghoulybox(*target->actor) && target->bytes[0x93C] != 0)
        {
            if (runtime_player_can_attach(context.eligibility, *target))
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
    if (get(context.mind, mind_field::target_record) == 0)
    {
        return true;
    }
    if (get(context.body, body_field::move_holds) == 0 && get(context.body, body_field::move_hold_time) == 0.f)
    {
        if (get(request, target_action_request::forced) != 0 || context.target->bytes[0x15] == 0)
        {
            return dispatch_runtime_mind_target_request(selection(), 0xFFFFFFFFu,
                                                        get(request, target_action_request::action)) != 1;
        }
        if (get(context.actor_bytes, player_field::input_device) != 0)
        {
            reset_runtime_input_device(input, input_context);
        }
    }
    return false;
}

// Xbox 0x0007BC80
u32 update_runtime_cinema_mind_target_position(RuntimeCinemaMindMovement& context, std::span<const u8> request)
{
    if (get(context.mind, mind_field::target_record) == 0)
    {
        return 1;
    }
    auto& target = *context.target;
    if (get(request, target_position_request::needs_movable) != 0 && !runtime_mind_target_is_movable(target))
    {
        return 0;
    }
    const i32 floor = get(target.bytes, mind_target_field::floor);
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
        if (!isolated && (get(target.bytes, mind_target_field::flags) & 0x10) != 0)
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
    if (get(request, target_position_request::lead) > 0.f)
    {
        const Vec3 position = actor_world_position(context.actor);
        const float dx = destination.x - position.x;
        const float dy = destination.y - position.y;
        const float dz = destination.z - position.z;
        if ((get(target.bytes, mind_target_field::direction).z * dz + dx * get(target.bytes, mind_target_field::direction).x) + dy * 0.f < 0.f)
        {
            const float offset = get(request, target_position_request::lead);
            destination.x = get(target.bytes, mind_target_field::direction).x * offset + destination.x;
            destination.y = offset * 0.f + destination.y;
            destination.z = get(target.bytes, mind_target_field::direction).z * offset + destination.z;
        }
    }
    const Vec3 position = actor_world_position(context.actor);
    i32 cell;
    float radius = 0.f;
    if (entity_detail::navigation_path_clear(position, destination, 1, 0, 0, 0, nullptr, &cell, i16(context.actor.class_key)))
    {
        const float body_radius = context.eligibility.body_actor.radius_xz;
        radius = runtime_player_body_attachment_radius(context.eligibility)
            + get(context.body, body_field::radius_scale) * body_radius;
        radius = radius + get(request, target_position_request::radius);
        radius = radius + read_f32(shape, 0x1C);
        radius = radius + 1.f;
    }
    u32 failed;
    const u32 result = drive_runtime_player_navigation(
        context.navigation, destination, floor, radius, get(request, target_position_request::approach),
        get(request, target_position_request::speed),
        isolated ? 0 : (get(target.bytes, mind_target_field::flags) & 0x10) >> 4, failed);
    if (get(request, target_position_request::waits_for_idle) != 0 && result == 1)
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
    if (get(context.mind, mind_field::target_record) == 0)
    {
        return 1;
    }
    auto& target = *context.target;
    if (get(request, vertical_target_request::needs_movable) != 0 && !runtime_mind_target_is_movable(target))
    {
        return 0;
    }
    Vec3 destination = point(target.bytes, 0x20);
    if (get(request, vertical_target_request::lead) > 0.f)
    {
        const Vec3 position = actor_world_position(context.actor);
        const float dx = position.x - get(target.bytes, mind_target_field::shape_position).x;
        const float dy = position.y - get(target.bytes, mind_target_field::shape_position).y;
        const float dz = position.z - get(target.bytes, mind_target_field::shape_position).z;
        if ((get(target.bytes, mind_target_field::direction).z * dz + dx * get(target.bytes, mind_target_field::direction).x) + dy * 0.f < 0.f)
        {
            const float offset = get(request, vertical_target_request::lead);
            destination.x = get(target.bytes, mind_target_field::direction).x * offset + destination.x;
            destination.y = offset * 0.f + destination.y;
            destination.z = get(target.bytes, mind_target_field::direction).z * offset + destination.z;
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
            context.eligibility.attached, target.actor) + get(request, vertical_target_request::radius) + 1.f;
    }
    u32 failed;
    const u32 result = drive_runtime_mind_vertical_navigation(
        context.navigation, destination, get(target.bytes, mind_target_field::floor),
        get(request, vertical_target_request::height), get(request, vertical_target_request::tolerance), radius,
        get(request, vertical_target_request::approach), get(request, vertical_target_request::speed),
        get(request, vertical_target_request::vertical_speed),
        (get(target.bytes, mind_target_field::flags) & 0x10) >> 4, failed);
    if (get(request, vertical_target_request::waits_for_idle) != 0 && result == 1)
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
    if (get(context.actor_bytes, player_field::mind) == 0 || context.escape.target.data() == nullptr)
    {
        return 1;
    }
    const float distance = runtime_body_target_surface_distance(context.actor, context.eligibility, context.escape.target);
    if (distance > get(request, escape_request::safe_distance))
    {
        return 1;
    }
    const float step = runtime_frame_step(context.clock);
    const float rate = runtime_actor_rate(context.actor);
    const float timer = get(request, escape_request::retry_timer) - rate * step;
    set(request, escape_request::retry_timer, timer);
    if (timer <= 0.f)
    {
        set(request, escape_request::retry_timer, timer + 2.f);
        Vec3 selected;
        if (!select_runtime_mind_escape_position(context.escape, get(request, escape_request::heading),
            get(request, escape_request::min_distance), get(request, escape_request::max_distance), selected))
        {
            set(request, escape_request::retry_timer, 0.f);
            return 0;
        }
        set(context.mind, mind_field::goal_point, selected);
    }
    const Vec3 destination = get(context.mind, mind_field::goal_point);
    float heading;
    randomize_runtime_mind_escape_heading(context.actor, context.escape.target, destination, heading, context.random);
    set(request, escape_request::heading, heading);
    auto* costs = suspend_runtime_mind_target_costs(context.actor_bytes, context.body,
        context.actor, context.escape.target);
    u32 failed;
    const u32 result = drive_runtime_player_navigation(context.navigation, destination, -1,
        2.5f, 0.f, get(request, escape_request::speed), 0, failed);
    if (costs != nullptr)
    {
        restore_runtime_mind_target_costs(context.actor_bytes, costs);
    }
    if (result == 1)
    {
        set(request, escape_request::retry_timer, 0.f);
    }
    return 0;
}

// Xbox 0x0007B2F0
u32 update_runtime_cinema_mind_vertical_escape_motion(RuntimeCinemaMindMovement& context, std::span<u8> request)
{
    if (get(context.actor_bytes, player_field::mind) == 0 || context.escape.target.data() == nullptr)
    {
        return 1;
    }
    const float distance = runtime_body_target_surface_distance(context.actor, context.eligibility, context.escape.target);
    if (distance > get(request, vertical_escape_request::safe_distance))
    {
        return 1;
    }
    const float step = runtime_frame_step(context.clock);
    const float rate = runtime_actor_rate(context.actor);
    const float timer = get(request, vertical_escape_request::retry_timer) - rate * step;
    set(request, vertical_escape_request::retry_timer, timer);
    if (timer <= 0.f)
    {
        set(request, vertical_escape_request::retry_timer, timer + 2.f);
        Vec3 selected;
        if (!select_runtime_mind_escape_position(context.escape, get(request, vertical_escape_request::heading),
                                                 get(request, vertical_escape_request::min_distance),
                                                 get(request, vertical_escape_request::max_distance), selected))
        {
            set(request, vertical_escape_request::retry_timer, 0.f);
            return 0;
        }
        set(context.mind, mind_field::goal_point, selected);
    }
    const Vec3 destination = get(context.mind, mind_field::goal_point);
    float heading;
    randomize_runtime_mind_escape_heading(context.actor, context.escape.target, destination, heading, context.random);
    set(request, vertical_escape_request::heading, heading);
    auto* costs = suspend_runtime_mind_target_costs(context.actor_bytes, context.body,
        context.actor, context.escape.target);
    const i32 floor =
        entity_detail::navigation_floor_index(destination, i32(get(context.actor_bytes, player_field::floor)), 0);
    u32 failed;
    const u32 result = drive_runtime_mind_vertical_navigation(context.navigation, destination, floor,
        get(request, vertical_escape_request::height), get(request, vertical_escape_request::tolerance), 2.5f, 0.f,
        get(request, vertical_escape_request::speed), get(request, vertical_escape_request::vertical_speed), 0, failed);
    if (costs != nullptr)
    {
        restore_runtime_mind_target_costs(context.actor_bytes, costs);
    }
    if (result == 1)
    {
        set(request, vertical_escape_request::retry_timer, 0.f);
    }
    return 0;
}

// Xbox 0x0007B5A0
u32 update_runtime_cinema_mind_vertical_position(RuntimeCinemaMindMovement& context, std::span<const u8> request)
{
    const Vec3 destination = get(context.mind, mind_field::goal_point);
    const i32 floor =
        entity_detail::navigation_floor_index(destination, i32(get(context.actor_bytes, player_field::floor)), 0);
    u32 failed;
    const u32 result = drive_runtime_mind_vertical_navigation(
        context.navigation, destination, floor, get(request, vertical_position_request::height),
        get(request, vertical_position_request::tolerance), get(request, vertical_position_request::radius) + 1.f,
        get(request, vertical_position_request::approach), get(request, vertical_position_request::speed),
        get(request, vertical_position_request::vertical_speed), 0, failed);
    if (result == 0 && runtime_player_navigation_fallback(context.navigation, destination))
    {
        return 1;
    }
    return result;
}

// Xbox 0x00136680
u32 update_runtime_mind_vertical_position_request(RuntimeCinemaMindMovement& context, std::span<const u8> request)
{
    const Vec3 destination = get(request, vertical_point_request::point);
    const i32 floor =
        entity_detail::navigation_floor_index(destination, i32(get(context.actor_bytes, player_field::floor)), 0);
    u32 failed;
    const u32 result = drive_runtime_mind_vertical_navigation(
        context.navigation, destination, floor, get(request, vertical_point_request::height),
        get(request, vertical_point_request::tolerance), get(request, vertical_point_request::radius) + 2.5f,
        get(request, vertical_point_request::approach), get(request, vertical_point_request::speed),
        get(request, vertical_point_request::vertical_speed), 0, failed);
    if (result == 0 && runtime_player_navigation_fallback(context.navigation, get(context.mind, mind_field::goal_point)))
    {
        return 1;
    }
    return result;
}

// Xbox 0x0007BC20
u32 update_runtime_cinema_mind_position(RuntimeCinemaMindMovement& context, std::span<const u8> request)
{
    const Vec3 destination = get(context.mind, mind_field::goal_point);
    u32 failed;
    const u32 result = drive_runtime_player_navigation(context.navigation, destination, -1,
        float(static_cast<long double>(get(request, position_request::radius)) + 1.L),
        get(request, position_request::approach), get(request, position_request::speed), 0, failed);
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
    if (entity_detail::select_navigation_spawn_position(
            i32(get(context.actor_bytes, player_field::floor)), nullptr, position,
            get(request, random_position_request::min_distance), get(request, random_position_request::max_distance),
            context.required, context.allowed, 0x1C, i16(context.actor.class_key), selected, context.random,
            context.occupancy_initial))
    {
        set(context.mind, mind_field::goal_point, selected);
        return 1;
    }
    return 4;
}

// Xbox 0x0007B940
u32 select_runtime_cinema_mind_heading_position(RuntimeCinemaMindMovement& context, std::span<const u8> request)
{
    const Vec3 position = actor_world_position(context.actor);
    const float heading = randomize_runtime_navigation_heading(
        context.actor.euler.y, get(request, heading_position_request::spread), context.random);
    Vec3 selected;
    RuntimeNavigationHeadingSearch search{
        position,
        i32(get(context.actor_bytes, player_field::floor)),
        get(request, heading_position_request::min_distance),
        float(static_cast<long double>(get(request, heading_position_request::max_distance)) + .5L),
        context.required,
        context.allowed,
        &selected,
        1,
        0x1C,
        i16(context.actor.class_key),
        context.random,
        context.occupancy_initial};
    const float increment = get(request, heading_position_request::step);
    const i32 attempts = _mm_cvttss_si32(_mm_set_ss(std::bit_cast<float>(0x40490FDBu) / increment));
    if (search_runtime_navigation_headings(search, heading, increment, attempts))
    {
        set(context.mind, mind_field::goal_point, selected);
        return 1;
    }
    return 4;
}
}
