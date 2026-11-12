#if !defined(GBTG_RETAIL_GAME_ONLY)
// Rare Replay's faster tick (rare_replay.h).
#include "rare_replay.h"

#include "gbtg/log.h"

#include <algorithm>
#include <cmath>

namespace gbtg
{
namespace
{
bool g_enabled = false;
bool g_tick_per_frame = false;
bool g_retail_tick = true;
float g_seconds = 0.f; // Time toward the next retail tick.
u32 g_retail_frame = 0;
float g_fraction = 1.f;
}

void enable_rare_replay(bool tick_per_frame)
{
    g_enabled = true;
    g_tick_per_frame = tick_per_frame;
    g_seconds = 0.f;
    g_retail_frame = 0;
    log::info(tick_per_frame ? "rare replay: the logic ticks once a frame"
                             : "rare replay: the logic ticks at 60 Hz");
}

bool rare_replay_tick_per_frame()
{
    return g_enabled && g_tick_per_frame;
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
    // Steps of any length, counted in seconds (1/60 s at 60 Hz; any length once a frame). A step of a retail tick
    // (1/30 s) or more completes one retail tick and keeps no remainder, as retail counts a slow frame as one tick;
    // shorter steps add up to one.
    constexpr float kRetailTick = 1.f / 30.f;
    constexpr float kSlack = 1e-4f; // Two 1/60 s steps make one retail tick despite rounding.
    g_fraction = std::min(1.f, step_seconds / kRetailTick);
    g_seconds += step_seconds;
    g_retail_tick = g_seconds >= kRetailTick - kSlack;
    if (g_retail_tick)
    {
        g_seconds = step_seconds >= kRetailTick - kSlack ? 0.f : std::max(0.f, g_seconds - kRetailTick);
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
