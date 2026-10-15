#pragma once
#include "gbtg/types.h"

namespace gbtg
{
// Xbox 0x00101730: interpolate each packed color byte, clamp, then truncate.
u32 menu_color(u32 selected, u32 rest, float fraction); // Xbox 0x00101730
}
