#include "body_orientation.h"
#include "player_mind_feedback.h"
#include "player_mind_events.h"
#include "actor_transform.h"
#include "../camera/entity_camera.h"
#include "gbtg/anim.h"
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

// Xbox 0x001068E0, EBX tilt, EDI roll
i32 runtime_direction_lean(const Vec3& direction, float heading, float& tilt, float& roll)
{
    roll = 0.f;
    tilt = 0.f;
    float yaw = 0.f;
    float pitch = 0.f;
    const i32 live = direction_to_yaw_pitch(direction.x, direction.y, direction.z, yaw, pitch); // 0x00105E20
    if (live == 0)
    {
        return 0;
    }
    constexpr float pi = std::bit_cast<float>(0x40490FDBu); // DAT_0032B148.
    constexpr float tau = std::bit_cast<float>(0x40C90FDBu); // DAT_0032B298.
    const long double wrapped = animation_remainder(yaw - heading, tau); // 0x00152BE0
    float turn = float(wrapped);
    if (static_cast<long double>(-pi) > wrapped) // DAT_0032B29C.
    {
        turn = turn + tau;
    }
    if (!(turn < pi))
    {
        turn = turn - tau;
    }
    const float away = engine_absolute(turn); // 0x00152BB0
    constexpr float quarter = 1.5707964f; // DAT_0032B140.
    const float lean = pitch + quarter;
    const float low = 0.f - lean;
    const float value = (low - lean) * (away * std::bit_cast<float>(0x3EA2F983u)) + lean; // DAT_0032B43C, 1/pi.
    float result = value;
    if (low > lean)
    {
        if (value > low)
        {
            result = low;
        }
        else if (!(lean <= value))
        {
            result = lean;
        }
    }
    else if (low > value)
    {
        result = low;
    }
    else if (!(value <= lean))
    {
        result = lean;
    }
    tilt = result;
    constexpr float scale = std::bit_cast<float>(0x3F22F983u); // DAT_0032B438, 2/pi.
    float side = lean;
    if (quarter > away)
    {
        const float swing = away * scale * lean;
        if (lean > 0.f)
        {
            side = swing > lean ? lean : (0.f <= swing ? swing : 0.f);
        }
        else
        {
            side = lean > swing ? lean : (swing > 0.f ? 0.f : swing);
        }
    }
    else
    {
        const float swing = (away - quarter) * scale * low + lean;
        if (0.f > lean)
        {
            side = swing > 0.f ? 0.f : (lean <= swing ? swing : lean);
        }
        else
        {
            side = 0.f > swing ? 0.f : (swing > lean ? lean : swing);
        }
    }
    roll = side;
    if (turn > 0.f)
    {
        roll = 0.f - roll;
    }
    return live;
}

// Xbox 0x00134720
void settle_runtime_body_turn(std::span<u8> body, const Actor& actor)
{
    float x = 0.f;
    float y = read_f32(body, 0x730);
    float z = 0.f;
    if (body[0x19E] != 0)
    {
        const Vec3 direction{read_f32(body, 0x184), read_f32(body, 0x188), read_f32(body, 0x18C)};
        runtime_direction_lean(direction, actor.euler.y, x, z); // 0x001068E0, actor +0x18
    }
    std::memcpy(body.data() + 0x72C, &x, 4);
    std::memcpy(body.data() + 0x730, &y, 4);
    std::memcpy(body.data() + 0x734, &z, 4);
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
