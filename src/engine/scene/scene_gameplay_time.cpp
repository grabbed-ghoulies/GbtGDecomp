#include "scene_gameplay_time.h"
#if !defined(GBTG_RETAIL_GAME_ONLY)
#include "../runtime/rare_replay.h"
#endif
#include "../entities/entity.h"

#include <cstring>

namespace gbtg
{
#if !defined(GBTG_RETAIL_GAME_ONLY)
namespace
{
bool g_gameplay_added = false; // Rare Replay: whether this tick has added its step to the gameplay time.
}
#endif

// Xbox 0x0012B1F0
void advance_scene_gameplay_time(std::span<u8> timers, const RuntimeActorClock& clock,
    u32 flags)
{
    const float step = runtime_frame_step(clock);
#if !defined(GBTG_RETAIL_GAME_ONLY)
    g_gameplay_added = false;
#endif
    u32 frame = read_u32(timers, 0x10) + 1u;
#if !defined(GBTG_RETAIL_GAME_ONLY)
    // Rare Replay: +0x10 and +0xC count retail ticks.
    if (!rare_replay_retail_tick())
    {
        frame = read_u32(timers, 0x10);
    }
#endif
    const float first = read_f32(timers, 0) + step;
    std::memcpy(timers.data(), &first, 4);
    std::memcpy(timers.data() + 0x10, &frame, 4);
    const float second = read_f32(timers, 4) + step;
    std::memcpy(timers.data() + 4, &second, 4);
    if ((flags & 0x027001E0u) == 0)
    {
        const float gameplay = read_f32(timers, 8) + step;
        std::memcpy(timers.data() + 8, &gameplay, 4);
        std::memcpy(timers.data() + 0xC, &frame, 4);
#if !defined(GBTG_RETAIL_GAME_ONLY)
        g_gameplay_added = true;
#endif
    }
}

bool scene_gameplay_time_due(i32 tick, i32 mark)
{
#if !defined(GBTG_RETAIL_GAME_ONLY)
    if (rare_replay_enabled())
    {
        return !g_gameplay_added;
    }
#endif
    return tick != mark;
}

void note_scene_gameplay_time_added()
{
#if !defined(GBTG_RETAIL_GAME_ONLY)
    g_gameplay_added = true;
#endif
}
}
