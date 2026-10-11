#include "camera_pulse.h"

namespace gbtg
{
// Xbox 0x00061960
void start_runtime_camera_pulse(float duration, float first, float second, float third, float fourth,
    u32 mode, const u32& runtime_flags, RuntimeCameraPulse& pulse)
{
    if ((runtime_flags & 0x8000) == 0)
    {
        pulse.remaining = duration;
        pulse.duration = duration;
        pulse.first = first;
        pulse.second = second;
        pulse.third = third;
        pulse.fourth = fourth;
        pulse.active = 1;
        pulse.mode = mode;
    }
}
}
