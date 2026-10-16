#pragma once

#include "../scene/scene_types.h"
#include "scene_transition.h"

namespace gbtg
{
struct Game;
// DAT_00546328: the transition's row. Its address is the handler argument (g_scene_transition).
extern i32& g_546328;

// DAT_0054632C: the transition's phase. 1 runs the countdown and the handler, 3 runs the handler. A nonzero value
// is the clear gate.
extern i32& g_54632c;

// DAT_0050484C. Incremented when it is below 1 and DAT_00546344 is above 0.
extern i32 g_50484c;

// DAT_0050E7A0. Counted when the scene state leaves or enters 1 or 2.
extern i32 g_50e7a0;

// DAT_00510CEC. Counted up when DAT_0054632C is 0 on entry to 0x0012C5D0.
extern i32 g_510cec;

// DAT_004DEA24. Counted with that gate, and once more on the way out.
extern i32 g_4dea24;

// DAT_00510F80. Scene record +0x128. Cleared when the phase is 1.
extern i32& g_510f80;

// DAT_00510F7C. Scene record +0x124. Cleared when the phase is 3.
extern i32& g_510f7c;

// DAT_00507DF4. 0x0011CE40 stores 0.
extern i32 g_507df4;

// DAT_00510CE8. Nonzero clears bit 0x20 of DAT_0050EA9C, then stores 0.
extern i32 g_510ce8;

// DAT_00510E40. Nonzero is EBX for 0x00035C00, then stores 0.
extern void* g_510e40;

// DAT_00510E34. Nonzero decrements DAT_004DEA24, then stores 0.
extern i32 g_510e34;

// DAT_0050E458: a one-shot scene name that replaces the next directory lookup's (0x00116220).
extern char g_50e458[0x80];

// The game the frame's script pump (0x0011E4C0) runs.
extern Game* g_transition_game;
} // namespace gbtg
