#include "event_timing.h"
#include <cstring>

namespace gbtg
{
// Xbox 0x0004C6C0
bool select_cutscene_event_phase(const u8* event, i32 ending, float current,
    float previous, i32 restricted, u32& phase)
{
    u16 kind;
    float start;
    float end;
    std::memcpy(&kind, event + 0xC, sizeof(kind));
    std::memcpy(&start, event + 4, sizeof(start));
    std::memcpy(&end, event + 8, sizeof(end));
    if (ending != 0 && kind != 0xF) // DAT_00423624
    {
        return false;
    }
    if (restricted != 0 && kind != 0xE) // DAT_00423628
    {
        return false;
    }
    if (end == 0.f)
    {
        if (previous <= start && (current > start || ending != 0))
        {
            phase = ending != 0 ? 3 : 0;
            return true;
        }
    }
    else
    {
        if (previous <= start && (current > start || ending != 0))
        {
            phase = ending != 0 ? 3 : 0;
            return true;
        }
        if (previous <= end && (current > end || ending != 0))
        {
            phase = ending != 0 ? 3 : 1;
            return true;
        }
        if (current > start && (end > current || ending != 0))
        {
            phase = ending != 0 ? 3 : 2;
            return true;
        }
    }
    return false;
}
}
