#include "color_blend.h"
#include "../audio/listener_settings.h"

namespace gbtg
{
// Xbox 0x00101730: interpolate each packed color byte, clamp, then truncate.
u32 menu_color(u32 selected, u32 rest, float fraction)
{
    const float inverse = 1.f - fraction;
    const auto channel = [inverse, fraction](u32 first, u32 second)
    {
        float value = static_cast<float>(second) * inverse + static_cast<float>(first) * fraction;
        if (value < 0.f)
        {
            value = 0.f;
        }
        else if (255.f < value)
        {
            value = 255.f;
        }
        return static_cast<u8>(truncate_runtime_float(value));
    };
    const u32 red = channel(selected & 0xFF, rest & 0xFF);
    const u32 green = channel((selected >> 8) & 0xFF, (rest >> 8) & 0xFF);
    const u32 blue = channel((selected >> 16) & 0xFF, (rest >> 16) & 0xFF);
    const u32 alpha = channel(selected >> 24, rest >> 24);
    return red | (green << 8) | (blue << 16) | (alpha << 24);
}
}
