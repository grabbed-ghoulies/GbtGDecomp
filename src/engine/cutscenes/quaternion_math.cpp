#include "quaternion_math.h"
#include "gbtg/anim.h"
#include <bit>

namespace gbtg
{
// Xbox 0x001535C0
void cutscene_sine_cosine(float angle, float& sine, float& cosine)
{
    constexpr float pi = std::bit_cast<float>(0x40490FDBu);
    constexpr float turn = std::bit_cast<float>(0x40C90FDBu);
    if (-pi <= angle)
    {
        while (pi < angle)
        {
            angle = angle - turn;
        }
    }
    else
    {
        do
        {
            angle = turn + angle;
        }
        while (angle < -pi);
    }
    float value = animation_absolute(angle);
    bool swap;
    bool negate;
    if (value <= std::bit_cast<float>(0x3F490FDBu))
    {
        swap = false;
        negate = false;
    }
    else
    {
        if (std::bit_cast<float>(0x4016CBE4u) < value)
        {
            value = value - pi;
            swap = false;
            negate = true;
        }
        else
        {
            swap = true;
            value = std::bit_cast<float>(0x3FC90FDBu) - value;
            negate = false;
        }
    }
    const float squared = value * value;
    float sin_value = ((squared * std::bit_cast<float>(0x3C051003u)
        - std::bit_cast<float>(0x3E2A999Au)) * squared
        + std::bit_cast<float>(0x3F7FFFACu)) * value;
    float cos_value = ((std::bit_cast<float>(0x3D2A9E75u)
        - squared * std::bit_cast<float>(0x3AB21287u)) * squared
        - std::bit_cast<float>(0x3EFFFFD0u)) * squared + 1.f;
    if (!swap)
    {
        if (!negate)
        {
            cosine = cos_value;
            if (angle < 0.f)
            {
                sin_value = 0.f - sin_value;
            }
            sine = sin_value;
            return;
        }
        cosine = 0.f - cos_value;
        if (0.f <= angle)
        {
            sin_value = 0.f - sin_value;
        }
        sine = sin_value;
        return;
    }
    if (!negate)
    {
        cosine = sin_value;
        if (angle < 0.f)
        {
            sine = 0.f - cos_value;
            return;
        }
        sine = cos_value;
        return;
    }
    cosine = 0.f - sin_value;
    if (0.f <= angle)
    {
        cos_value = 0.f - cos_value;
    }
    sine = cos_value;
}

// Xbox 0x00154500
void ordered_euler_quaternion(u32 order, const Vec3& euler, Quat& output)
{
    float first = euler.x;
    float third = euler.z;
    if ((order & 0x10) != 0)
    {
        first = euler.z;
        third = euler.x;
    }
    first = .5f * first;
    third = .5f * third;
    float second = .5f * euler.y;
    const u32 reverse = order & 4;
    if (reverse != 0)
    {
        second = 0.f - second;
    }
    first = static_cast<float>(animation_wrap_angle(first));
    second = static_cast<float>(animation_wrap_angle(second));
    third = static_cast<float>(animation_wrap_angle(third));
    float sin_first, cos_first, sin_second, cos_second, sin_third, cos_third;
    cutscene_sine_cosine(first, sin_first, cos_first);
    cutscene_sine_cosine(second, sin_second, cos_second);
    cutscene_sine_cosine(third, sin_third, cos_third);
    // DAT_0032E330, 32 rows, including the retail zero rows.
    constexpr u32 orders[] =
    {
        0x00020100, 0x00000201, 0x00010002, 0,
        0x00010200, 0x00020001, 0x00000102, 0,
        0x00020100, 0x00000201, 0x00010002, 0,
        0x00010200, 0x00020001, 0x00000102, 0,
        0x00020100, 0x00000201, 0x00010002, 0,
        0x00010200, 0x00020001, 0x00000102, 0,
        0x00020100, 0x00000201, 0x00010002, 0,
        0x00010200, 0x00020001, 0x00000102, 0
    };
    const u32 row = orders[order];
    const u32 a = row & 0xFF;
    const u32 b = (row >> 8) & 0xFF;
    const u32 c = (row >> 16) & 0xFF;
    const float cc = cos_third * cos_first;
    const float sc = sin_third * cos_first;
    const float cs = cos_third * sin_first;
    const float ss = sin_third * sin_first;
    std::array<float, 4> result{output.x, output.y, output.z, output.w};
    if ((order & 8) == 0)
    {
        result[a] = cs * cos_second - sc * sin_second;
        result[b] = ss * cos_second + cc * sin_second;
        result[c] = sc * cos_second - cs * sin_second;
        result[3] = ss * sin_second + cc * cos_second;
    }
    else
    {
        result[a] = (cs + sc) * cos_second;
        result[b] = (ss + cc) * sin_second;
        result[c] = (sc - cs) * sin_second;
        result[3] = (cc - ss) * cos_second;
    }
    if (reverse != 0)
    {
        result[b] = 0.f - result[b];
    }
    output = {result[0], result[1], result[2], result[3]};
}
}
