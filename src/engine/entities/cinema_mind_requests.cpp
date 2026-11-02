#include "cinema_mind_requests.h"
#include "player_mind_targets.h"
#include "mind_fields.h"
#include "body_orientation.h"
#include "../navigation/mind_escape_position.h"
#include "body_animation_commands.h"
#include "body_damage_motion.h"
#include "player_mind_control.h"
#include "player_mind_feedback.h"
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
void value(std::span<u8> bytes, size_t offset, float number)
{
    std::memcpy(bytes.data() + offset, &number, 4);
}
void random_value(std::span<u8> request, size_t range, size_t output, u32& random)
{
    const std::array<float, 2> limits{read_f32(request, range), read_f32(request, range + 4)};
    float number;
    randomize_runtime_player_request_value(limits, number, random);
    value(request, output, number);
}
}

// Xbox 0x0007B110
u32 select_runtime_cinema_mind_escape_request(std::span<u8> mind,
    std::span<const u8> request, RuntimeMindEscapePosition& context)
{
    Vec3 point;
    if (select_runtime_mind_escape_position(context, read_f32(request, 0x1C),
        read_f32(request, 0x14), read_f32(request, 0x18), point))
    {
        set(mind, mind_field::goal_point, point);
        return 1;
    }
    return 4;
}

// Xbox 0x0007B800
u32 face_runtime_cinema_mind_target(std::span<const u8> mind, std::span<u8> body,
    const Actor& actor, std::span<const u8> target)
{
    if (get(mind, mind_field::target_record) == 0)
    {
        return 1;
    }
    if (read_u16(body, 0x30) == 0 && read_f32(body, 0x34) == 0.f)
    {
        const Vec3 point{get(target, mind_target_field::shape_position).x, get(target, mind_target_field::shape_position).y, get(target, mind_target_field::shape_position).z};
        face_runtime_body_point(body, actor, point);
    }
    return 0;
}

// Xbox 0x0007C580
u32 initialize_runtime_cinema_mind_request(Actor& actor, std::span<const u8> target,
    std::span<u8> request, u32& random)
{
    const u16 kind = read_u16(request, 8);
    if (kind == 0x29)
    {
        value(request, 0x18, runtime_body_random_range(random,
            read_f32(request, 0x10), read_f32(request, 0x14)));
    }
    switch (kind)
    {
    case 0xD:
        if (target.data() == nullptr)
        {
            return 0;
        }
        value(request, 0x28, runtime_body_random_range(random,
            read_f32(request, 0x18), read_f32(request, 0x1C)));
        value(request, 0x2C, runtime_actor_bound_angle(target, actor) + std::bit_cast<float>(0x40490FDBu));
        return 1;
    case 0xE:
        if (target.data() == nullptr)
        {
            return 0;
        }
        value(request, 0x34, runtime_body_random_range(random,
            read_f32(request, 0x20), read_f32(request, 0x24)));
        value(request, 0x38, runtime_actor_bound_angle(target, actor) + std::bit_cast<float>(0x40490FDBu));
        return 1;
    case 0xF:
        random_value(request, 0x10, 0x2C, random);
        return 1;
    case 0x10:
        random_value(request, 0x1C, 0x38, random);
        return 1;
    case 0x11:
        random_value(request, 0x10, 0x38, random);
        return 1;
    case 0x24:
        {
            float angle;
            if (target.data() == nullptr || read_u32(request, 0x20) != 0)
            {
                angle = actor.euler.y;
            }
            else
            {
                const Vec3 point{get(target, mind_target_field::shape_position).x, get(target, mind_target_field::shape_position).y, get(target, mind_target_field::shape_position).z};
                angle = runtime_actor_direction_to_point(actor, point);
            }
            random = random * 0x19660Du + 0x3C6EF35Fu;
            const float fraction = std::bit_cast<float>((random & 0x7FFFFFu) | 0x3F800000u) - 1.f;
            const float low = 0.f - read_f32(request, 0x14);
            const float selected = (((read_f32(request, 0x14) - low) * fraction +
                read_f32(request, 0x10)) + low) * std::bit_cast<float>(0x3C8EFA35u) + angle;
            value(request, 0x28, selected);
            return 1;
        }
    case 0x2D:
        random_value(request, 0x10, 0x20, random);
        return 1;
    case 0x2E:
        random_value(request, 0x1C, 0x2C, random);
        break;
    case 0x2F:
        random_value(request, 0x10, 0x2C, random);
        break;
    default:
        break;
    }
    return 1;
}
}
