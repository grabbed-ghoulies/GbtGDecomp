#pragma once

#include "frontend_types.h"

namespace gbtg
{
namespace frontend_detail
{
// 0x0005F6E2. Delay is (rng_float in [1, 2) - 1) * DAT_0032B104.
float banner_delay(unsigned& seed);

// Xbox 0x00080A30 creates the attract banner. 0x000809C0 places the widget:
// x = DAT_00509730 (0x000FC190, width/2), y = DAT_0050972C - font[+0x10] * DAT_0032B5B4.
// param_8 = 1, so 0x0005EF40 draws from anchor_x - measured_width/2.
// 0x00080C10 counts DAT_0032B284 (7s) and builds the widget again.
void draw_start_banner(const Game& g); // Xbox 0x00080A30

} // namespace frontend_detail
} // namespace gbtg
