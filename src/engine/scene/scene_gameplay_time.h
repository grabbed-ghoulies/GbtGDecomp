#pragma once

#include "../entities/actor_clock.h"

namespace gbtg
{
void advance_scene_gameplay_time(std::span<u8> timers, const RuntimeActorClock& clock,
    u32 flags); // Xbox 0x0012B1F0; DAT_004312C8 through DAT_004312D8.
}
