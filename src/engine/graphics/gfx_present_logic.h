#pragma once

#include "gbtg/types.h"

namespace gbtg
{

// Retail present-time logic every gfx backend runs around its own clear, draw and swap.
// Xbox 0x00033A90: the D3DDevice_Clear mask for the frame.
u32 gfx_present_clear_flags();
// Xbox 0x00033A90: overlay records (0x00106CD0) and camera slots (0x00033A20).
void gfx_present_overlays();
void gfx_present_cameras();
// Xbox 0x00033A90: DAT_0043427C, called with 0 before D3DDevice_Swap and 1 after.
void gfx_present_swap_notify(int phase);

} // namespace gbtg
