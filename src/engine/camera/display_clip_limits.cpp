#include "display_clip_limits.h"
#include <bit>

namespace gbtg
{
// Xbox 0x0013C490
void extend_display_clip_limits(std::span<const float, 12> transform,
    const DisplayClipBounds& bounds, float& far_clip, float& near_clip)
{
    float limit = 1500.f; // DAT_0032B238.
    if (bounds.active != 0)
    {
        const auto& points = bounds.corners;
        const float x = transform[8];
        const float y = transform[9];
        const float z = transform[10];
        const float translation = transform[11];
        const float first = 0.f - (((x * points[0].x + points[0].y * y) + points[0].z * z) + translation);
        const float second = 0.f - (((x * points[1].x + points[1].y * y) + points[1].z * z) + translation);
        const float third = 0.f - (((x * points[2].x + points[2].y * y) + points[2].z * z) + translation);
        const float fourth = 0.f - (((x * points[3].x + points[3].y * y) + points[3].z * z) + translation);
        const float fifth = 0.f - (((points[4].y * y + points[4].x * x) + points[4].z * z) + translation);
        const float sixth = 0.f - (((points[5].y * y + points[5].z * z) + points[5].x * x) + translation);
        const float seventh = 0.f - (((points[6].y * y + points[6].z * z) + points[6].x * x) + translation);
        const float eighth = 0.f - (((points[7].y * y + points[7].z * z) + points[7].x * x) + translation);
        float maximum = first;
        float minimum = second;
        if (!(first > second))
        {
            if (second > first)
            {
                maximum = second;
            }
            minimum = first;
        }
        auto extend = [&minimum, &maximum](float distance)
        {
            if (minimum > distance)
            {
                minimum = distance;
            }
            else if (distance > maximum)
            {
                maximum = distance;
            }
        };
        extend(third);
        extend(fourth);
        extend(fifth);
        extend(sixth);
        extend(seventh);
        if (!(minimum > eighth))
        {
            if (eighth > maximum)
            {
                maximum = eighth;
            }
        }
        limit = maximum + 100.f; // DAT_0032B12C.
    }
    near_clip = 0.5f; // DAT_0032B0F4.
    if (limit > far_clip)
    {
        far_clip = limit;
    }
    const float minimum_far = near_clip + 1.f;
    if (minimum_far > far_clip)
    {
        far_clip = minimum_far;
    }
    far_clip *= std::bit_cast<float>(0x3F8CCCCDu); // DAT_0032B1A0.
}
}
