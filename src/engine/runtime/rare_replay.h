#pragma once
// Rare Replay: the logic ticks faster than retail's 30 Hz (the rare_replay plugin feature, docs/PLUGINS.md). Retail
// presents with D3DPRESENT_INTERVAL_TWO, so each tick is two 60 Hz fields and the step (0x000FC030) is 1/30 s. With
// Rare Replay the platform loop ticks once per presented frame, the step the real time since the last tick
// ([rare_replay] TickPerFrame = 1, the default), or every 60 Hz field, a step of 1/60 s, as the Rare Replay release
// does (TickPerFrame = 0).
// The game scales its motion and timers by the step, but some code counts ticks: those sites ask
// rare_replay_retail_tick(), true on each tick that completes one of retail's 1/30 s ticks, so their counts keep
// retail's meaning. Non-retail builds only; with it off every tick is a retail tick.
#if !defined(GBTG_RETAIL_GAME_ONLY)
#include "gbtg/types.h"

namespace gbtg
{

void enable_rare_replay(bool tick_per_frame);
// Whether an interactive run ticks once per presented frame (Rare Replay on and TickPerFrame = 1).
bool rare_replay_tick_per_frame();
bool rare_replay_enabled();
// The 60 Hz fields a tick takes when the step is not longer: 1 with Rare Replay, else retail's 2.
int rare_replay_swap_interval();

// frame_tick, after 0x000FC030 sets the step: counts the step's fields toward retail ticks.
void advance_rare_replay(float step_seconds);
// Whether this tick completes a retail tick: always without Rare Replay, else once its steps add up to 1/30 s.
bool rare_replay_retail_tick();
// The retail tick count to use where retail reads the frame counter (DAT_004DEA58) as a 1/30 s clock: `frame`
// itself without Rare Replay, else the count of completed retail ticks.
const u32& rare_replay_retail_frame(const u32& frame);
// The part of a retail tick this tick is: 1 without Rare Replay, the step over 1/30 s with it (0.5 at 60 Hz).
float rare_replay_tick_fraction();

} // namespace gbtg
#endif
