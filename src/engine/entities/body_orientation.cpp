#include "body_orientation.h"
#include "player_mind_feedback.h"
#include "player_mind_events.h"
#include "actor_transform.h"
#include <bit>
#include <cstring>

namespace gbtg
{
// Xbox 0x00041AE0
float runtime_actor_relative_point_angle(const Actor& actor, const Vec3& point)
{
    const float angle = runtime_actor_direction_to_point(actor, point);
    const float difference = float(static_cast<long double>(angle) - actor.euler.y);
    const long double remainder = animation_remainder(difference, std::bit_cast<float>(0x40C90FDBu));
    float result = float(remainder);
    if (remainder < static_cast<long double>(std::bit_cast<float>(0xC0490FDBu)))
    {
        result += std::bit_cast<float>(0x40C90FDBu);
    }
    if (result >= std::bit_cast<float>(0x40490FDBu))
    {
        result -= std::bit_cast<float>(0x40C90FDBu);
    }
    return result;
}

// Xbox 0x00134510
void face_runtime_body_point(std::span<u8> body, const Actor& actor, const Vec3& point)
{
    if (read_u16(body, 0x30) == 0 && read_f32(body, 0x34) == 0.f)
    {
        const float angle = runtime_actor_direction_to_point(actor, point);
        const Vec3 rotation{0.f, angle, 0.f};
        std::memcpy(body.data() + 0x72C, &rotation, sizeof(rotation));
    }
}

// Xbox 0x001345F0
void face_runtime_body_target(std::span<u8> body, const Actor& actor,
    std::span<const u8> target)
{
    if (read_u16(body, 0x30) == 0 && read_f32(body, 0x34) == 0.f)
    {
        const Vec3 point{read_f32(target, 0x20), read_f32(target, 0x24), read_f32(target, 0x28)};
        face_runtime_body_point(body, actor, point);
    }
}

// Xbox 0x00134720
void settle_runtime_body_turn(std::span<u8> body, const Actor& actor)
{
    const u32 kept = read_u32(body, 0x730);
    if (body[0x19E] != 0)
    {
        throw Error("untranslated Xbox 0x001068E0 reached from 0x00134720 (body +0x19E set)");
    }
    const float zero = 0.f;
    std::memcpy(body.data() + 0x72C, &zero, 4);
    std::memcpy(body.data() + 0x730, &kept, 4);
    std::memcpy(body.data() + 0x734, &zero, 4);
}

// Xbox 0x000197B0
float runtime_body_point_heading(std::span<const u8> body, const Actor& actor)
{
    const Vec3 point{read_f32(body, 0x3C), read_f32(body, 0x40), read_f32(body, 0x44)};
    const Vec3 position = actor_world_position(actor);
    const Vec3 direction{point.x - position.x, point.y - position.y, point.z - position.z};
    float angle = 0.f;
    runtime_direction_angle(direction, angle);
    return angle;
}
}
