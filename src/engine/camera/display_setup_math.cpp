#include "display_setup_math.h"
#include "../cutscenes/quaternion_math.h"
#include "gbtg/anim.h"
#include <bit>

namespace gbtg
{
// Xbox 0x00153D80
void conjugate_display_quaternion(const Quat& input, Quat& output)
{
    output.x = 0.f - input.x;
    output.y = 0.f - input.y;
    output.z = 0.f - input.z;
    output.w = input.w;
}
// Xbox 0x001541B0
void normalize_display_quaternion(const Quat& input, Quat& output)
{
    const float squared = ((input.x * input.x + input.y * input.y) + input.z * input.z) + input.w * input.w;
    if (animation_absolute(squared - 1.f) > std::bit_cast<float>(0x3727C5ACu))
    {
        const long double factor = retail_reciprocal_sqrt(squared);
        output.x = static_cast<float>(factor * input.x);
        output.y = static_cast<float>(factor * input.y);
        output.z = static_cast<float>(factor * input.z);
        output.w = static_cast<float>(factor * input.w);
        return;
    }
    if (&input != &output)
    {
        output.x = input.x;
        output.y = input.y;
        output.z = input.z;
        output.w = input.w;
    }
}
// Xbox 0x00104910
void make_display_quaternion_matrix(const Quat& q, float* m)
{
    const float squared = ((q.x * q.x + q.y * q.y) + q.z * q.z) + q.w * q.w;
    const float factor = 2.f / squared;
    const float zs = q.z * factor;
    const float xs = q.x * factor;
    const float ys = q.y * factor;
    const float xx = q.x * xs;
    const float xy = q.x * ys;
    const float xz = q.x * zs;
    const float xw = xs * q.w;
    const float yw = ys * q.w;
    const float zw = zs * q.w;
    const float zz = q.z * zs;
    const float yy = q.y * ys;
    const float yz = q.y * zs;
    m[1] = xy - zw;
    m[4] = xy + zw;
    m[2] = xz + yw;
    m[5] = 1.f - (zz + xx);
    m[0] = 1.f - (zz + yy);
    m[3] = 0.f;
    m[6] = yz - xw;
    m[7] = 0.f;
    m[8] = xz - yw;
    m[9] = yz + xw;
    m[10] = 1.f - (yy + xx);
    m[11] = 0.f;
}
// Xbox 0x00154270
void extract_display_euler(const float* m, u32 order, float& first,
    float& second, float& third)
{
    constexpr u32 rows[]
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
    const u32 a = rows[order] & 0xFF;
    const u32 b = (rows[order] >> 8) & 0xFF;
    const u32 c = (rows[order] >> 16) & 0xFF;
    if ((order & 8) == 0)
    {
        const u32 index = a + b * 4;
        const float diagonal = m[a * 5];
        const float off_diagonal = m[index];
        const float length = static_cast<float>(retail_square_root(diagonal * diagonal + off_diagonal * off_diagonal));
        if (!(length > std::bit_cast<float>(0x36000000u)))
        {
            first = static_cast<float>(retail_atan2(0.f - m[c + b * 4], m[b * 5]));
            second = static_cast<float>(retail_atan2(0.f - m[a + c * 4], length));
            third = 0.f;
        }
        else
        {
            first = static_cast<float>(retail_atan2(m[b + c * 4], m[c * 5]));
            second = static_cast<float>(retail_atan2(0.f - m[a + c * 4], length));
            third = static_cast<float>(retail_atan2(m[index], m[a * 5]));
        }
    }
    else
    {
        const u32 first_index = c + a * 4;
        const u32 second_index = b + a * 4;
        const float diagonal = m[second_index];
        const float off_diagonal = m[first_index];
        const float length = static_cast<float>(retail_square_root(diagonal * diagonal + off_diagonal * off_diagonal));
        if (!(length > std::bit_cast<float>(0x36000000u)))
        {
            first = static_cast<float>(retail_atan2(0.f - m[c + b * 4], m[b * 5]));
            second = static_cast<float>(retail_atan2(length, m[a * 5]));
            third = 0.f;
        }
        else
        {
            first = static_cast<float>(retail_atan2(m[second_index], m[first_index]));
            second = static_cast<float>(retail_atan2(length, m[a * 5]));
            third = static_cast<float>(retail_atan2(m[a + b * 4], 0.f - m[a + c * 4]));
        }
    }
    if ((order & 4) != 0)
    {
        first = 0.f - first;
        second = 0.f - second;
        third = 0.f - third;
    }
    if ((order & 0x10) != 0)
    {
        const float saved = first;
        first = third;
        third = saved;
    }
}
// Xbox 0x001546E0
void extract_display_quaternion_euler(const Quat& q, u32 order,
    float& first, float& second, float& third)
{
    float m[12];
    const float zs = q.z * 2.f;
    const float ys = q.y * 2.f;
    const float xx = q.x * (q.x * 2.f);
    const float xw = q.w * (q.x * 2.f);
    m[0] = 1.f - (q.z * zs + q.y * ys);
    m[4] = q.x * ys - q.w * zs;
    m[1] = q.x * ys + q.w * zs;
    m[8] = q.x * zs + q.w * ys;
    m[5] = 1.f - (q.z * zs + xx);
    m[10] = 1.f - (q.y * ys + xx);
    m[9] = q.y * zs - xw;
    m[2] = q.x * zs - q.w * ys;
    m[6] = q.y * zs + xw;
    m[3] = 0.f;
    m[7] = 0.f;
    m[11] = 0.f;
    extract_display_euler(m, order, first, second, third);
}
// Xbox 0x00104D80
void expand_display_affine(const float* m, float* output)
{
    output[0] = m[0];
    output[1] = m[4];
    output[2] = m[8];
    output[4] = m[1];
    output[5] = m[5];
    output[6] = m[9];
    output[8] = m[2];
    output[9] = m[6];
    output[10] = m[10];
    output[12] = m[3];
    output[13] = m[7];
    output[11] = 0.f;
    output[7] = 0.f;
    output[3] = 0.f;
    output[14] = m[11];
    output[15] = 1.f;
}
// Xbox 0x00104A70
void transform_display_points(const float* m, const float* input, float* output, u32 count)
{
    u32 processed = 0;
    if (static_cast<i32>(count) > 3)
    {
        u32 groups = ((count - 4) >> 2) + 1;
        processed = groups * 4;
        do
        {
            const float x0 = input[0], y0 = input[1], z0 = input[2];
            output[0] = ((y0 * m[1] + m[2] * z0) + m[0] * x0) + m[3];
            output[1] = ((y0 * m[5] + m[6] * z0) + m[4] * x0) + m[7];
            output[2] = ((x0 * m[8] + y0 * m[9]) + z0 * m[10]) + m[11];
            const float x1 = input[3], y1 = input[4], z1 = input[5];
            output[3] = ((y1 * m[1] + m[0] * x1) + z1 * m[2]) + m[3];
            output[4] = ((y1 * m[5] + z1 * m[6]) + m[4] * x1) + m[7];
            output[5] = ((y1 * m[9] + z1 * m[10]) + m[8] * x1) + m[11];
            const float x2 = input[6], y2 = input[7], z2 = input[8];
            output[6] = ((m[2] * z2 + m[0] * x2) + y2 * m[1]) + m[3];
            output[7] = ((m[5] * y2 + m[6] * z2) + m[4] * x2) + m[7];
            output[8] = ((x2 * m[8] + y2 * m[9]) + z2 * m[10]) + m[11];
            const float x3 = input[9], y3 = input[10], z3 = input[11];
            output[9] = ((z3 * m[2] + x3 * m[0]) + y3 * m[1]) + m[3];
            output[10] = ((m[4] * x3 + m[5] * y3) + m[6] * z3) + m[7];
            output[11] = ((y3 * m[9] + z3 * m[10]) + x3 * m[8]) + m[11];
            input += 12;
            output += 12;
            --groups;
        }
        while (groups != 0);
    }
    if (processed < count)
    {
        u32 remaining = count - processed;
        do
        {
            const float x = input[0], y = input[1], z = input[2];
            output[0] = ((y * m[1] + m[2] * z) + m[0] * x) + m[3];
            output[1] = ((y * m[5] + m[6] * z) + m[4] * x) + m[7];
            output[2] = ((x * m[8] + y * m[9]) + z * m[10]) + m[11];
            input += 3;
            output += 3;
            --remaining;
        }
        while (remaining != 0);
    }
}
// Xbox 0x00152860
long double display_tangent(float value)
{
    float angle = static_cast<float>(animation_wrap_angle(value));
    constexpr float half_pi = std::bit_cast<float>(0x3FC90FDBu);
    constexpr float quarter_pi = std::bit_cast<float>(0x3F490FDBu);
    constexpr float pi = std::bit_cast<float>(0x40490FDBu);
    bool reciprocal = false;
    if (!(angle >= 0.f))
    {
        if (!(-half_pi > angle))
        {
            if (-quarter_pi > angle)
            {
                angle = half_pi + angle;
                reciprocal = true;
            }
        }
        else
        {
            angle = pi - angle;
            if (angle > quarter_pi)
            {
                angle -= half_pi;
                reciprocal = true;
            }
        }
    }
    else if (!(angle > half_pi))
    {
        if (angle > quarter_pi)
        {
            angle -= half_pi;
            reciprocal = true;
        }
    }
    else
    {
        angle -= pi;
        if (-quarter_pi > angle)
        {
            angle = half_pi + angle;
            reciprocal = true;
        }
    }
    const float squared = angle * angle;
    float result = ((((squared * std::bit_cast<float>(0x3D410D88u)
        + std::bit_cast<float>(0x3D12C05Bu)) * squared + std::bit_cast<float>(0x3E0DF16Au))
        * squared + std::bit_cast<float>(0x3EAA5AC5u)) * squared + std::bit_cast<float>(0x3F8000A3u)) * angle;
    if (reciprocal)
    {
        result = 0.f - 1.f / result;
    }
    return result;
}
}
