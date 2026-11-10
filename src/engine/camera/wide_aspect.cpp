#if !defined(GBTG_RETAIL_GAME_ONLY)
// The wide aspect row (wide_aspect.h).
#include "wide_aspect.h"

#include "display_view.h"
#include "gbtg/log.h"

#include <algorithm>

namespace gbtg
{
namespace
{
bool g_requested = false;
float g_requested_ratio = 0.f; // 0: the window's.
float g_ratio = 0.f; // The applied wide row's aspect, 0 when none.
float g_frame = 4.f / 3.f; // The presented frame's aspect: 4:3, 16:9 or the wide row's.
float g_hud = 0.f; // The HUD's aspect, 0 to stretch.
i32 g_hud_scopes = 0;
bool g_page_hold = false; // hold_wide_aspect_for_page: the 2D fits the frame's 4:3 middle.

constexpr float kSixteenNine = 16.f / 9.f;
constexpr float kWidest = 4.f; // Wider than 32:9 (3.56) is refused.
}

namespace entity_detail
{
i32 widescreen_aspect_row()
{
    return g_ratio > 0.f ? kWideAspectRow : 1;
}

float wide_aspect_ratio()
{
    return g_ratio;
}

i32 aspect_sensor_row(i32 row)
{
    return row == kWideAspectRow ? 1 : row;
}

float aspect_row_ratio(i32 row, const float* table)
{
    return row == kWideAspectRow ? g_ratio : table[row];
}
} // namespace entity_detail

void hold_wide_aspect_for_page(bool held)
{
    g_page_hold = held;
}

void hold_wide_aspect_for_paused_page(bool book_shown, i32 paused, i32 phase)
{
    hold_wide_aspect_for_page(g_ratio > 0.f && book_shown && paused != 0 && phase != 4 && phase != 5);
}

bool wide_aspect_page_held()
{
    return g_page_hold;
}

void request_wide_aspect(float ratio)
{
    g_requested = true;
    g_requested_ratio = ratio;
}

void request_hud_aspect(float aspect)
{
    g_hud = aspect;
}

HudAspectScope::HudAspectScope(bool active)
    : active(active)
{
    if (active)
    {
        ++g_hud_scopes;
    }
}

HudAspectScope::~HudAspectScope()
{
    if (active)
    {
        --g_hud_scopes;
    }
}

void hud_viewport(const u32* fitted, u32* output)
{
    std::copy_n(fitted, 4, output);
    // The fit is in the render target's pixels: the display's size (0x000F5900's DAT_005096E8 / DAT_005096EA copy).
    const auto& display = *entity_detail::g_display_view;
    // While the pause's page is captured, all of the 2D fits the frame's 4:3 middle, which the page shows.
    const float hud = g_page_hold ? 4.f / 3.f : g_hud;
    if ((g_hud_scopes <= 0 && !g_page_hold) || hud <= 0.f || fitted[3] == 0 || display.width <= 0
        || display.height <= 0)
    {
        return;
    }
    // The fit's shape on screen: its pixels are the frame's aspect over the target's.
    const float pixel = g_frame / (static_cast<float>(display.width) / static_cast<float>(display.height));
    const float shown = static_cast<float>(fitted[2]) / static_cast<float>(fitted[3]) * pixel;
    if (shown <= hud + 0.001f)
    {
        return;
    }
    const u32 width = static_cast<u32>(static_cast<float>(fitted[2]) * hud / shown + 0.5f);
    output[0] = fitted[0] + (fitted[2] - width) / 2;
    output[2] = width;
}

float apply_wide_aspect(int width, int height, bool widescreen)
{
    g_frame = widescreen ? 16.f / 9.f : 4.f / 3.f;
    if (!g_requested)
    {
        return 0.f;
    }
    if (!widescreen)
    {
        log::warn("wide_aspect: needs [Video] Widescreen=1; the frame stays 4:3");
        return 0.f;
    }
    float ratio = g_requested_ratio;
    if (ratio <= 0.f)
    {
        if (width <= 0 || height <= 0)
        {
            return 0.f;
        }
        ratio = static_cast<float>(width) / static_cast<float>(height);
    }
    if (ratio <= kSixteenNine + 0.01f)
    {
        log::info("wide_aspect: %.3f is not wider than 16:9; the frame stays 16:9", ratio);
        return 0.f;
    }
    if (ratio > kWidest)
    {
        log::warn("wide_aspect: %.3f is wider than %.1f; using %.1f", ratio, kWidest, kWidest);
        ratio = kWidest;
    }
    g_ratio = ratio;
    g_frame = ratio;
    // Boot (0x00033030) has already chosen row 1 for the widescreen display.
    if (entity_detail::g_aspect_preset == 1)
    {
        entity_detail::g_aspect_preset = entity_detail::kWideAspectRow;
    }
    log::info("wide_aspect: the display row is %.3f:1", ratio);
    return ratio;
}

} // namespace gbtg
#endif
