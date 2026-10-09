#include "interpolation.h"

namespace gbtg
{
// Xbox 0x001037F0
float interpolate_clamped_range(float end_time, float current, float start_time,
    float start, float end)
{
    if (end_time == start_time)
    {
        return end;
    }
    const float value = ((current - start_time) / (end_time - start_time)) * (end - start) + start;
    if (!(end > start))
    {
        if (end > value)
        {
            return end;
        }
        if (value > start)
        {
            return start;
        }
    }
    else
    {
        if (value > end)
        {
            return end;
        }
        if (start > value)
        {
            return start;
        }
    }
    return value;
}
}
