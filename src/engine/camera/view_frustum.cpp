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
}
