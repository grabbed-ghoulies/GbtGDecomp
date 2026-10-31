#pragma once

#include "gbtg/game.h"

namespace gbtg
{
// Host input boundary off Windows, in place of GetAsyncKeyState and XInputGetState (0x001555B0): the keyboard
// stand-ins and up to four SDL gamepads, written into the same packets the Windows path fills. Events must
// already have been pumped this frame (gfx_pump).
void poll_sdl_input(InputState& in);
}
