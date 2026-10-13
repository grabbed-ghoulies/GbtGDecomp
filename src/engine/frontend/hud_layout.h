#pragma once

#include "gbtg/types.h"

namespace gbtg
{
// The HUD safe area 0x000FC190 derives from the display size (DAT_005096E8 / DAT_005096EA).
struct RuntimeHudLayout
{
    float tight_height; // DAT_00509708, height * 0.9.
    float tight_width; // DAT_0050970C, width * 0.9.
    float loose_height; // DAT_00509710, height * 0.85.
    float loose_width; // DAT_00509714, width * 0.85, or 0.93 on an exact-aspect display.
    float safe_height; // DAT_00509718.
    float safe_width; // DAT_0050971C.
    float left; // DAT_00509720.
    float top; // DAT_00509724.
    float right; // DAT_00509728.
    float bottom; // DAT_0050972C.
    float center_x; // DAT_00509730.
    float center_y; // DAT_00509734.
    float text_height; // DAT_00509738, safe height - 25.
    float text_width; // DAT_0050973C, safe width - 25; the dialogue wrap width.
    float inner_left; // DAT_00509740.
    float inner_top; // DAT_00509744.
    float inner_right; // DAT_00509748.
    float inner_bottom; // DAT_0050974C.
    float anchor_x; // DAT_00509750.
    float anchor_y; // DAT_00509754.
};
void layout_runtime_hud(u8 exact_aspect, u16 width, u16 height,
    RuntimeHudLayout& layout); // Xbox 0x000FC190, DAT_00509704 exact aspect
}
