#include "cinema_mind_request_update.h"
#include "player_fields.h"
#include "mind_fields.h"
#include "mind_request_fields.h"
#include "cinema_mind_requests.h"
#include "body_orientation.h"
#include "body_message_helpers.h"
#include "../graphics/actor_effect_stream.h"
#include "gbtg/anim.h"
#include <bit>

namespace gbtg
{
namespace
{
Vec3 point(std::span<const u8> bytes, size_t offset)
{
    return {read_f32(bytes, offset), read_f32(bytes, offset + 4), read_f32(bytes, offset + 8)};
}
void sound(RuntimeCinemaMindRequestUpdate& context)
{
    auto& movement = context.movement;
    update_runtime_cinema_mind_sound(movement.mind, context.body, context.sound_resource,
        movement.actor, movement.clock, movement.random, context.sounds);
}
}

// Xbox 0x0007C0E0, facing cases 0x25..0x27.
i32 update_runtime_cinema_mind_facing_request(std::span<const u8> mind, std::span<u8> body,
    const Actor& actor, std::span<const u8> target, std::span<const u8> request)
{
    const u16 kind = get(request, mind_request_field::kind);
    Vec3 destination;
    float degrees;
    if (kind == 0x27)
    {
        if (mind_record(mind).target_record == 0)
        {
            return 1;
        }
        face_runtime_body_target(body, actor, target);
        destination = point(target, 0x20);
        degrees = get(request, facing_request::degrees);
    }
    else
    {
        destination = kind == 0x25 ? mind_record(mind).goal_point : get(request, facing_request::point);
        face_runtime_body_point(body, actor, destination);
        degrees = kind == 0x26 ? get(request, facing_request::point_degrees) : get(request, facing_request::degrees);
    }
    const float angle = runtime_actor_relative_point_angle(actor, destination);
    const float limit = float(static_cast<long double>(degrees) * std::bit_cast<float>(0x3C8EFA35u));
    return static_cast<long double>(limit) > animation_absolute(angle) ? 1 : 0;
}

// Xbox 0x0007C0E0
i32 update_runtime_cinema_mind_request(RuntimeCinemaMindRequestUpdate& context, std::span<u8> request)
{
    auto& movement = context.movement;
    switch (get(request, mind_request_field::kind))
    {
    case 2:
        return update_runtime_cinema_mind_target_action(movement, request, context.chains,
            context.input, context.input_context);
    case 8:
    {
        const u32 flag = runtime_body_flag_name_index(reinterpret_cast<const char*>(request.data() + 0x10),
            context.parent.flag_names);
        if (!runtime_body_bit_set(context.parent.flags, flag))
        {
            return 1;
        }
        if (!actor_event_chance(movement.random, read_f32(request, 0x40)))
        {
            return 1;
        }
        [[fallthrough]];
    }
    case 7:
        select_runtime_body_action(context.body, context.effect_action_key(), context.actions);
        return 1;
    case 0xD:
        sound(context);
        return i32(update_runtime_cinema_mind_escape_motion(movement, request));
    case 0xE:
        sound(context);
        return i32(update_runtime_cinema_mind_vertical_escape_motion(movement, request));
    case 0xF:
        return i32(update_runtime_cinema_mind_vertical_position(movement, request));
    case 0x10:
        sound(context);
        return i32(update_runtime_mind_vertical_position_request(movement, request));
    case 0x11:
        return i32(update_runtime_cinema_mind_vertical_target(movement, request));
    case 0x13:
        return i32(select_runtime_cinema_mind_escape_request(movement.mind, request, movement.escape));
    case 0x14:
        return i32(select_runtime_cinema_mind_random_position(movement, request));
    case 0x15:
        return i32(select_runtime_cinema_mind_heading_position(movement, request));
    case 0x17:
        return i32(face_runtime_cinema_mind_target(movement.mind, context.body, movement.actor,
            context.parent.target_bytes));
    case 0x18:
        return i32(request_runtime_cinema_mind_spawn_attachment(movement, context.spawn.target,
            context.attach, context.body, context.actions));
    case 0x19:
    {
        const Vec3 destination = player_record(movement.actor_bytes).start_position;
        const i32 floor = entity_detail::navigation_floor_index(destination, -1, 0);
        u32 failed;
        const u32 result = drive_runtime_player_navigation(movement.navigation, destination, floor,
            5.f, 30.f, 1.f, 0, failed);
        if (result == 0 && runtime_player_navigation_fallback(movement.navigation, destination))
        {
            return 1;
        }
        return i32(result);
    }
    case 0x24:
        return i32(update_runtime_cinema_mind_timed_motion(movement, request, context.damage_tables,
            context.mode_scales));
    case 0x25:
    case 0x26:
    case 0x27:
        return update_runtime_cinema_mind_facing_request(movement.mind, context.body,
            movement.actor, context.parent.target_bytes, request);
    case 0x2D:
        sound(context);
        return i32(update_runtime_cinema_mind_position(movement, request));
    case 0x2E:
        sound(context);
        return i32(handle_runtime_player_navigation_request(movement.navigation, request));
    case 0x2F:
        sound(context);
        return i32(update_runtime_cinema_mind_target_position(movement, request));
    case 0x30:
        sound(context);
        return i32(update_runtime_cinema_mind_spawn_position(context.spawn));
    case 0x31:
        select_runtime_body_action(context.body, context.contact_action_key(), context.actions);
        return 1;
    default:
        return update_runtime_parent_mind_request(context.parent, request);
    }
}
}
