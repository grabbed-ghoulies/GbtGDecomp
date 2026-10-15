#pragma once
#include "gbtg/types.h"

namespace gbtg
{
extern u32 g_shared_random; // DAT_0054DD7C, the game's shared LCG state (BSS).
float advance_runtime_random_fraction(u32& state); // Xbox 0x00105140
}
