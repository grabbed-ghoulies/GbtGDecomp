#include "hud_layout.h"

namespace gbtg
{
// Xbox 0x000FC190
void layout_runtime_hud(u8 exact_aspect, u16 width, u16 height, RuntimeHudLayout& layout)
{
    constexpr float kLoose = 0.85f; // DAT_0032B17C.
    constexpr float kWide = 0.93f; // DAT_0032B178.
    constexpr float kTight = 0.9f; // DAT_0032B174.
    constexpr float kHalf = 0.5f; // DAT_0032B0F4.
    constexpr float kTextInset = 25.f; // DAT_0032B170.
    constexpr float kEdgeInset = 12.5f; // DAT_0032B16C.
    const float horizontal = exact_aspect != 0 ? kWide : kLoose;
    const float h = float(height);
    const float w = float(width);
    const float loose_height = h * kLoose;
    const float loose_width = w * horizontal;
    const float tight_height = h * kTight;
    const float tight_width = w * kTight;
    layout.loose_height = loose_height;
    layout.loose_width = loose_width;
    layout.tight_height = tight_height;
    layout.tight_width = tight_width;
    float safe_height = tight_height;
    if (!(loose_height > tight_height))
    {
        safe_height = loose_height;
    }
    layout.safe_height = safe_height;
    float safe_width = tight_width;
    if (!(loose_width > tight_width))
    {
        safe_width = loose_width;
    }
    const float top = (h - safe_height) * kHalf;
    const float bottom = h - top;
    const float left = (w - safe_width) * kHalf;
    layout.bottom = bottom;
    const float center_x = safe_width * kHalf + left;
    const float center_y = safe_height * kHalf + top;
    const float right = w - left;
    layout.safe_width = safe_width;
    layout.left = left;
    layout.inner_left = left + kEdgeInset;
    layout.top = top;
    layout.right = right;
    layout.center_x = center_x;
    layout.center_y = center_y;
    layout.text_height = safe_height - kTextInset;
    layout.text_width = safe_width - kTextInset;
    layout.inner_top = top + kEdgeInset;
    layout.inner_right = right - kEdgeInset;
    layout.inner_bottom = bottom - kEdgeInset;
    layout.anchor_x = center_x;
    layout.anchor_y = center_y;
}
}
