#include "view_frustum.h"
#include <array>

namespace gbtg
{
// Xbox 0x00159AD0
u32 classify_runtime_sphere_frustum(std::span<const float, 42> frustum, const Vec3& center, float radius,
    u8& plane)
{
    std::array<float, 6> distances;
    u8 index = plane;
    for (u8 tested = 0; tested < 6; ++tested)
    {
        const float* normal = frustum.data() + 18 + size_t(index) * 4;
        const float distance = (normal[1] * center.y + normal[2] * center.z + normal[0] * center.x) - normal[3];
        distances[index] = distance;
        if (distance > radius)
        {
            plane = index;
            return 0;
        }
        ++index;
        if (index > 5)
        {
            index = 0;
        }
    }
    const float limit = 0.f - radius;
    for (const float distance : distances)
    {
        if (!(limit >= distance))
        {
            return 1;
        }
    }
    return 2;
}

// Xbox 0x00159BA0
u8 classify_runtime_box_frustum(std::span<const float, 42> frustum, const Vec3& first, const Vec3& second,
    u8& plane)
{
    u8 index = plane;
    u16 sides = 0;
    u8 tested = 0;
    while (true)
    {
        const float* normal = frustum.data() + 18 + size_t(index) * 4;
        float near_x;
        float far_x;
        if (normal[0] < 0.f)
        {
            near_x = second.x;
            far_x = first.x;
        }
        else
        {
            near_x = first.x;
            far_x = second.x;
        }
        float near_y;
        float far_y;
        if (normal[1] < 0.f)
        {
            near_y = second.y;
            far_y = first.y;
        }
        else
        {
            near_y = first.y;
            far_y = second.y;
        }
        float near_z;
        float far_z;
        if (normal[2] < 0.f)
        {
            near_z = second.z;
            far_z = first.z;
        }
        else
        {
            near_z = first.z;
            far_z = second.z;
        }
        u16 side;
        if (((normal[1] * near_y + normal[0] * near_x) + near_z * normal[2]) - normal[3] > 0.f)
        {
            side = 3;
        }
        else if (((normal[1] * far_y + normal[0] * far_x) + far_z * normal[2]) - normal[3] > 0.f)
        {
            side = 1;
        }
        else
        {
            side = 0;
        }
        sides = u16(sides | side);
        if (sides == 3)
        {
            break;
        }
        ++index;
        if (index > 5)
        {
            index = 0;
        }
        ++tested;
        if (tested >= 6)
        {
            return u8((sides != 1 ? 1 : 0) + 1);
        }
    }
    plane = index;
    return 0;
}
}
