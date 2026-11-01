#if !defined(GBTG_RETAIL_GAME_ONLY)
// Rare Replay's 60 Hz tick (rare_replay.h).
#include "rare_replay.h"

#include "gbtg/log.h"

#include <algorithm>
#include <cmath>

namespace gbtg
{
namespace
{
bool g_enabled = false;
bool g_retail_tick = true;
i32 g_fields = 0; // 60 Hz fields toward the next retail tick.
u32 g_retail_frame = 0;
float g_fraction = 1.f;
}

void enable_rare_replay()
{
    g_enabled = true;
    g_fields = 0;
    g_retail_frame = 0;
    log::info("rare replay: the logic ticks at 60 Hz");
}

bool rare_replay_enabled()
{
    return g_enabled;
}

int rare_replay_swap_interval()
{
    return g_enabled ? 1 : 2;
}

void advance_rare_replay(float step_seconds)
{
    if (!g_enabled)
    {
        g_retail_tick = true;
        g_fraction = 1.f;
        return;
    }
    // A long step (a slow frame) is several fields; retail still counts it as one tick, so a tick of two or more
    // fields completes one retail tick and keeps no remainder.
    const i32 fields = std::max<i32>(1, i32(std::lround(step_seconds * 60.f)));
    g_fraction = std::min(1.f, float(fields) * 0.5f);
    g_fields += fields;
    g_retail_tick = g_fields >= 2;
    if (g_retail_tick)
    {
        g_fields = 0;
        ++g_retail_frame;
    }
}

bool rare_replay_retail_tick()
{
    return g_retail_tick;
}

const u32& rare_replay_retail_frame(const u32& frame)
{
    return g_enabled ? g_retail_frame : frame;
}

float rare_replay_tick_fraction()
{
    return g_fraction;
}
}
#endif
