#include "random_placement.h"
#include "gbtg/anim.h"
#include "gbtg/sound_runtime.h"
#include <bit>

namespace gbtg
{
namespace
{
float placement_fraction(u32& state)
{
    state = state * 0x19660Du + 0x3C6EF35Fu;
    return std::bit_cast<float>((state & 0x7FFFFFu) | 0x3F800000u) - 1.f;
}
}

// Xbox 0x00104FC0
float sample_runtime_angle_range(float upper, float lower, u32& random)
{
    constexpr float turn = std::bit_cast<float>(0x40C90FDBu);
    constexpr float pi = std::bit_cast<float>(0x40490FDBu);
    constexpr float negative_pi = std::bit_cast<float>(0xC0490FDBu);
    if (lower > upper)
    {
        lower -= turn;
        const float fraction = placement_fraction(random);
        const float angle = (upper - lower) * fraction + lower;
        const long double remainder = animation_remainder(angle, turn);
        float result = float(remainder);
        if (negative_pi > remainder)
        {
            result += turn;
        }
        if (result >= pi)
        {
            result -= turn;
        }
        return result;
    }
    if (upper > lower)
    {
        const float fraction = placement_fraction(random);
        return (upper - lower) * fraction + lower;
    }
    return lower;
}

// Xbox 0x00105550
void sample_runtime_rectangle_position(const Vec3& extent, Vec3& output, i32 surface, u32& random)
{
    const float x_upper = extent.x * .5f;
    const float x_lower = extent.x * -.5f;
    const float x_fraction = placement_fraction(random);
    output.x = (x_upper - x_lower) * x_fraction + x_lower;
    output.y = 0.f;
    const float z_upper = extent.y * .5f;
    const float z_lower = extent.y * -.5f;
    const float z_fraction = placement_fraction(random);
    output.z = (z_upper - z_lower) * z_fraction + z_lower;
    if (surface != 0)
    {
        switch (i32(placement_fraction(random) * 5.f))
        {
        case 0:
            output.x = extent.x * -.5f;
            break;
        case 1:
            output.x = extent.x * .5f;
            break;
        case 2:
            output.z = extent.y * -.5f;
            break;
        case 3:
        case 4:
            output.z = extent.y * .5f;
            break;
        }
    }
}

// Xbox 0x00105480
void sample_runtime_ellipse_position(const Vec3& extent, Vec3& output, i32 surface, u32& random)
{
    const float radius = surface != 0 ? 1.f : placement_fraction(random);
    const float angle = sample_runtime_angle_range(std::bit_cast<float>(0x40490FDBu),
        std::bit_cast<float>(0xC0490FDBu), random);
    output.z = 0.f;
    output.y = 0.f;
    output.x = 0.f;
    const float sine = float(retail_sin(angle));
    const float cosine = float(retail_cos(angle));
    const float x = radius * sine + output.x;
    const float z = radius * cosine + output.z;
    output.x = x;
    output.z = z;
    output.x = x * extent.x;
    output.z = extent.y * z;
}

// Xbox 0x001053B0
void sample_runtime_ellipsoid_position(const Vec3& extent, Vec3& output, i32 surface,
    float pitch_upper, float pitch_lower, float yaw_upper, float yaw_lower, u32& random)
{
    const float pitch = sample_runtime_angle_range(pitch_upper, pitch_lower, random);
    const float yaw = sample_runtime_angle_range(yaw_upper, yaw_lower, random);
    const float radius = surface != 0 ? 1.f : placement_fraction(random);
    Vec3 direction{};
    add_sound_direction(direction, pitch, yaw, radius);
    output.x = extent.x * direction.x;
    output.y = extent.y * direction.y;
    output.z = extent.z * direction.z;
}

// Xbox 0x001056B0
void sample_runtime_cylinder_position(const Vec3& extent, Vec3& output, i32 surface,
    float pitch_upper, float pitch_lower, u32& random)
{
    const float pitch = sample_runtime_angle_range(pitch_upper, pitch_lower, random);
    output.z = 0.f;
    output.y = 0.f;
    output.x = 0.f;
    add_sound_direction(output, pitch, 0.f, 1.f);
    const float x_upper = extent.x * .5f;
    const float x_lower = extent.x * -.5f;
    const float fraction = placement_fraction(random);
    output.x = (x_upper - x_lower) * fraction + x_lower;
    const float y_scale = surface != 0 ? extent.y : placement_fraction(random) * extent.y;
    output.y = output.y * y_scale;
    if (surface != 0)
    {
        output.z = extent.z * output.z;
        return;
    }
    const float z_scale = placement_fraction(random) * extent.z;
    output.z = z_scale * output.z;
}

// Xbox 0x00105170
void sample_runtime_line_position(Vec3& output, float extent, u32& random)
{
    const float fraction = placement_fraction(random);
    const float upper = extent * .5f;
    const float lower = extent * -.5f;
    output.x = (upper - lower) * fraction + lower;
    output.y = 0.f;
    output.z = 0.f;
}

// Xbox 0x001051E0
void sample_runtime_box_position(const Vec3& extent, Vec3& output, i32 surface, u32& random)
{
    const float* source[] = {&extent.x, &extent.y, &extent.z};
    float* destination[] = {&output.x, &output.y, &output.z};
    for (size_t axis = 0; axis < 3; ++axis)
    {
        const float upper = *source[axis] * .5f;
        const float lower = *source[axis] * -.5f;
        const float fraction = placement_fraction(random);
        *destination[axis] = (upper - lower) * fraction + lower;
    }
    if (surface != 0)
    {
        const i32 face = i32(placement_fraction(random) * 7.f);
        switch (face)
        {
        case 0:
            output.x = extent.x * -.5f;
            break;
        case 1:
            output.x = extent.x * .5f;
            break;
        case 2:
            output.y = extent.y * .5f;
            break;
        case 3:
            output.y = extent.y * -.5f;
            break;
        case 4:
            output.z = extent.z * -.5f;
            break;
        case 5:
        case 6:
            output.z = extent.z * .5f;
            break;
        }
    }
}
}
