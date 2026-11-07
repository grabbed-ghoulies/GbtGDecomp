#include "body_gaze.h"
#include <cstddef>
#include "gbtg/model_runtime.h"
#include "body_fields.h"
#include <bit>
#include <cstring>

namespace gbtg
{
// Xbox 0x00105D90
long double runtime_input_axes_angle(const std::array<float, 2>& axes)
{
    if (axes[0] == 0.f && axes[1] == 0.f)
    {
        return 0.f;
    }
    return retail_atan2(axes[1], axes[0]);
}

// Xbox 0x00100E40
float runtime_relative_angle(float angle, float reference)
{
    constexpr float turn = std::bit_cast<float>(0x40C90FDBu);
    constexpr float lower = std::bit_cast<float>(0xC0490FDBu);
    constexpr float upper = std::bit_cast<float>(0x40490FDBu);
    const long double remainder = animation_remainder(angle - reference, turn);
    float result = float(remainder);
    if (static_cast<long double>(lower) > remainder)
    {
        result += turn;
    }
    if (result >= upper)
    {
        result -= turn;
    }
    return result;
}

// Xbox 0x000251D0
Vec3 runtime_body_gaze_origin(std::span<const u8> body, const Actor& actor)
{
    if (actor.model != nullptr && actor.model->cpu.data() != nullptr)
    {
        const u32 joint = actor_lookup_joint_index(actor.model->cpu, 4) & 0xFF;
        if (joint != 0xFF && joint != 0xFFFFFFFF)
        {
            Vec3 output;
            actor_transform_joint_point(actor, u16(joint), actor_transform_origin, output);
            return output;
        }
    }
    Vec3 output = actor_world_position(actor);
    output.y = ((body_record(body).height_scale * actor.radius_y)
        * std::bit_cast<float>(0x3F4CCCCDu)) + output.y;
    return output;
}

// Xbox 0x00025250
void set_runtime_body_gaze_angle(std::span<u8> body, const Actor& actor, float angle)
{
    if (angle != 0.f)
    {
        constexpr float turn = std::bit_cast<float>(0x40C90FDBu);
        constexpr float lower = std::bit_cast<float>(0xC0490FDBu);
        constexpr float upper = std::bit_cast<float>(0x40490FDBu);
        long double remainder = animation_remainder(angle, turn);
        angle = float(remainder);
        if (static_cast<long double>(lower) > remainder)
        {
            angle += turn;
        }
        if (angle >= upper)
        {
            angle -= turn;
        }
        constexpr float minimum = std::bit_cast<float>(0xBF9C61AAu);
        constexpr float maximum = std::bit_cast<float>(0x3F9C61AAu);
        if (minimum > angle)
        {
            angle = minimum;
        }
        else if (angle > maximum)
        {
            angle = maximum;
        }
        const float world_angle = float(static_cast<long double>(angle) + actor.euler.y);
        remainder = animation_remainder(world_angle, turn);
        angle = float(remainder);
        if (static_cast<long double>(lower) > remainder)
        {
            angle += turn;
        }
        if (angle >= upper)
        {
            angle -= turn;
        }
        const Vec3 origin = actor_world_position(actor);
        const Vec3 target{float(retail_sin(angle) * 50.f + origin.x),
            actor.radius_y * body_record(body).height_scale + origin.y,
            float(retail_cos(angle) * 50.f + origin.z)};
        std::memcpy(body.data() + offsetof(BodyRecord, look_point.x), &target, sizeof(target));
        body_record(body).look_point_set = 1;
    }
}
}
