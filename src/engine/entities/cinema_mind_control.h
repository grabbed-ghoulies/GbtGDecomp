#pragma once
#include "actor_clock.h"
#include "gbtg/actor_runtime.h"

namespace gbtg
{
void refresh_runtime_cinema_mind_action_permission(std::span<u8> mind,
    std::span<const u8> actor, i32 allowed_count,
    std::span<const u8> allowed_rows); // Xbox 0x0007C4C0
void update_runtime_cinema_mind_sound(std::span<u8> mind,
    std::span<const u8> body, std::span<const u8> resource, Actor& actor,
    const RuntimeActorClock& clock, u32& random,
    RuntimeActorSoundQueues& sounds); // Xbox 0x0007CCD0
}
