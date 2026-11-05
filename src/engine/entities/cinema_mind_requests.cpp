#include "mind_request_fields.h"
#include "cinema_mind_requests.h"
#include "player_mind_targets.h"
#include "mind_fields.h"
#include "body_orientation.h"
#include "../navigation/mind_escape_position.h"
#include "body_animation_commands.h"
#include "body_damage_motion.h"
#include "player_mind_control.h"
#include "player_mind_feedback.h"
#include "body_fields.h"
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
template <typename Low, typename High, typename Output>
void random_value(std::span<u8> request, Low low, High high, Output output, u32& random)
{
    const std::array<float, 2> limits{get(request, low), get(request, high)};
    float number;
    randomize_runtime_player_request_value(limits, number, random);
    set(request, output, number);
}
}

// Xbox 0x0007B110
u32 select_runtime_cinema_mind_escape_request(std::span<u8> mind,
    std::span<const u8> request, RuntimeMindEscapePosition& context)
{
    Vec3 point;
    if (select_runtime_mind_escape_position(context, get(request, heading_position_request::step),
        get(request, heading_position_request::min_distance), get(request, heading_position_request::max_distance), point))
    {
        mind_record(mind).goal_point = point;
        return 1;
    }
    return 4;
}

// Xbox 0x0007B800
u32 face_runtime_cinema_mind_target(std::span<const u8> mind, std::span<u8> body,
    const Actor& actor, std::span<const u8> target)
{
    if (mind_record(mind).target_record == 0)
    {
        return 1;
    }
    if (body_record(body).move_holds == 0 && body_record(body).move_hold_time == 0.f)
    {
        const Vec3 point{mind_target_record(target).shape.position.x, mind_target_record(target).shape.position.y,
            mind_target_record(target).shape.position.z};
        face_runtime_body_point(body, actor, point);
    }
    return 0;
}

// Xbox 0x0007C580
u32 initialize_runtime_cinema_mind_request(Actor& actor, std::span<const u8> target,
    std::span<u8> request, u32& random)
{
    const u16 kind = get(request, mind_request_field::kind);
    if (kind == 0x29)
    {
        set(request, wait_request::duration, runtime_body_random_range(random,
            get(request, wait_request::duration_low), get(request, wait_request::duration_high)));
    }
    switch (kind)
    {
    case 0xD:
        if (target.data() == nullptr)
        {
            return 0;
        }
        set(request, escape_request::safe_distance, runtime_body_random_range(random,
            get(request, escape_request::safe_low), get(request, escape_request::safe_high)));
        set(request, escape_request::heading, runtime_actor_bound_angle(target, actor) + std::bit_cast<float>(0x40490FDBu));
        return 1;
    case 0xE:
        if (target.data() == nullptr)
        {
            return 0;
        }
        set(request, vertical_escape_request::safe_distance, runtime_body_random_range(random,
            get(request, vertical_escape_request::safe_low), get(request, vertical_escape_request::safe_high)));
        set(request, vertical_escape_request::heading, runtime_actor_bound_angle(target, actor) + std::bit_cast<float>(0x40490FDBu));
        return 1;
    case 0xF:
        random_value(request, vertical_position_request::speed_low, vertical_position_request::speed_high, vertical_position_request::speed, random);
        return 1;
    case 0x10:
        random_value(request, vertical_point_request::speed_low, vertical_point_request::speed_high, vertical_point_request::speed, random);
        return 1;
    case 0x11:
        random_value(request, vertical_target_request::speed_low, vertical_target_request::speed_high, vertical_target_request::speed, random);
        return 1;
    case 0x24:
        {
            float angle;
            if (target.data() == nullptr || get(request, timed_motion_request::own_heading) != 0)
            {
                angle = actor.euler.y;
            }
            else
            {
                const Vec3 point{mind_target_record(target).shape.position.x,
                    mind_target_record(target).shape.position.y, mind_target_record(target).shape.position.z};
                angle = runtime_actor_direction_to_point(actor, point);
            }
            random = random * 0x19660Du + 0x3C6EF35Fu;
            const float fraction = std::bit_cast<float>((random & 0x7FFFFFu) | 0x3F800000u) - 1.f;
            const float low = 0.f - get(request, timed_motion_request::heading_spread);
            const float selected = (((get(request, timed_motion_request::heading_spread) - low) * fraction +
                get(request, timed_motion_request::heading_offset)) + low) * std::bit_cast<float>(0x3C8EFA35u) + angle;
            set(request, timed_motion_request::heading, selected);
            return 1;
        }
    case 0x2D:
        random_value(request, position_request::speed_low, position_request::speed_high, position_request::speed, random);
        return 1;
    case 0x2E:
        random_value(request, navigation_request::speed_low, navigation_request::speed_high, navigation_request::speed, random);
        break;
    case 0x2F:
        random_value(request, target_position_request::speed_low, target_position_request::speed_high, target_position_request::speed, random);
        break;
    default:
        break;
    }
    return 1;
}
}
