#pragma once

#include "../entities/actor_clock.h"

namespace gbtg
{
void advance_scene_gameplay_time(std::span<u8> timers, const RuntimeActorClock& clock,
    u32 flags); // Xbox 0x0012B1F0; DAT_004312C8 through DAT_004312D8.

// The scene controls that run while the scene's clock is held (trophy heads 0x00081890, relics store 0x00081080)
// add the step to the gameplay time themselves when the clock's last advance (0x0012B1F0, after the controls) did
// not: retail tests the tick count (DAT_004312D8) against the count at the last add (DAT_004312D4). Rare Replay's
// tick count counts retail ticks, so there it asks whether the step was added since the clock's last advance.
bool scene_gameplay_time_due(i32 tick, i32 mark);
// A control added this tick's step.
void note_scene_gameplay_time_added();
}
