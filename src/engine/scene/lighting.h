#pragma once
#include "gbtg/types.h"

namespace gbtg
{
// The scene light-volume records (DAT_00509C20, 25 records of 0x25C bytes) and their draw order.
extern i32 g_light_filled; // DAT_0050D71C.
extern i32 g_light_filled_gate; // DAT_0050D720.
extern u8 g_light_order[0x19]; // DAT_0050D724.
}
