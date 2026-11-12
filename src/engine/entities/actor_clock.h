#pragma once
#include "gbtg/entity.h"
#include <span>

namespace gbtg
{
struct RuntimeActorClock
{
    const float& freeze; // DAT_004312DC.
    const float& dt; // DAT_005096C0.
    const float& scale; // DAT_004312C4.
    const u32& block_decay; // DAT_0050E768.
    std::span<const u8> decay_enabled; // DAT_0030F144.
    const u32& decay_index; // DAT_0051248C.
};

// The frame step: a freeze above zero (ordered) yields zero. Retail calls it once (Super Scary's wrong-press timer)
// and inlines the same expression everywhere else.
// Xbox 0x0012B270
inline float runtime_frame_step(const RuntimeActorClock& clock)
{
    if (clock.freeze > 0.f)
    {
        return 0.f;
    }
    return clock.dt * clock.scale;
}

// Inline retail rate: actor +0x930 above zero (ordered) yields zero, otherwise +0x92C.
inline float runtime_actor_rate(const Actor& actor)
{
    if (actor.animation_pause > 0.f)
    {
        return 0.f;
    }
    return actor.animation_rate;
}

inline bool runtime_decay_enabled(const RuntimeActorClock& clock)
{
    return clock.block_decay == 0 && clock.decay_enabled[clock.decay_index] != 0;
}

// Xbox 0x00042320
inline float runtime_actor_step(const Actor& actor, const RuntimeActorClock& clock)
{
    return runtime_actor_rate(actor) * runtime_frame_step(clock);
}
}
