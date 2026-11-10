#pragma once
// Wide aspect: a display wider than 16:9, such as 21:9 or 32:9 (the wide_aspect plugin feature, docs/PLUGINS.md).
// Retail's aspect table (0x0042A460) has rows 0 (4:3), 1 (16:9) and 2 (2.35, a camera letterbox), and boot
// (0x00033030) and the dialogue capture (0x00058150) only ever choose row 0 or 1 as the display's (DAT_005077F8).
// With a wide aspect applied, those writers choose kWideAspectRow instead of 1: a host row with the display's ratio
// and row 1's sensor height and default focal, so the vertical field of view stays 16:9's and the view widens.
// Views that keep their own row (a camera's flag 8, a cutscene's 4:3 or 16:9 slot) fit inside it as before.
// Non-retail builds only; with no wide aspect applied every choice is retail's.
#if !defined(GBTG_RETAIL_GAME_ONLY)
#include "gbtg/types.h"

namespace gbtg
{
namespace entity_detail
{
constexpr i32 kWideAspectRow = 4;

// The display row the retail writers choose on a widescreen display: 1, or kWideAspectRow with a wide aspect.
i32 widescreen_aspect_row();
// The wide row's aspect (width / height), 0 when none is applied.
float wide_aspect_ratio();
// The retail row whose sensor height and default focal `row` uses: row 1 for the wide row, else `row` itself.
i32 aspect_sensor_row(i32 row);
// The aspect of `row`, reading the wide row from the host and any other from `table` (a copy of 0x0042A460's
// aspect column).
float aspect_row_ratio(i32 row, const float* table);
} // namespace entity_detail

// The wide_aspect feature's boot hook: the requested aspect, 0 for the window's own when wider than 16:9.
void request_wide_aspect(float ratio);
// The pause's page (0x00048550) captures the frame onto a 4:3 page without the widening redraw the transitions'
// pages get, so with the wide row the paused scene came out squeezed. While held, every 2D draw fits the frame's 4:3
// middle (hud_viewport); the book's draw then widens the capture by the row's ratio / (4:3) as a transition's, and
// the page shows that middle: the scene in its own shape and the menu as at 4:3.
void hold_wide_aspect_for_page(bool held);
// The scene draw's hook: with the wide row, holds while the pause's book shows (`book_shown`) on a paused scene
// (DAT_00510CE8) in a phase (DAT_00510CF4) whose page 0x00048550 captures without widening, any but 4 and 5. The
// book's draw releases it before its widening redraw, and the scene draw after the frame.
void hold_wide_aspect_for_paused_page(bool book_shown, i32 paused, i32 phase);
bool wide_aspect_page_held();
// The HUD's shape ([wide_aspect] Hud): the 640x480 overlays drawn over gameplay (HUD, captions, prompts) fit an
// area of this aspect centred in the frame instead of stretching over it, as retail's do in widescreen. 0 stretches.
void request_hud_aspect(float aspect);

// Overlay draws inside a HudAspectScope use hud_viewport. The scene's overlay passes open one (its sprite queue
// 0x00122230 and text pools); a view already narrower than the HUD's aspect, such as a 4:3 dialogue page, is unchanged.
struct HudAspectScope
{
    explicit HudAspectScope(bool active = true); // An inactive scope changes nothing.
    ~HudAspectScope();
    bool active;
    HudAspectScope(const HudAspectScope&) = delete;
    HudAspectScope& operator=(const HudAspectScope&) = delete;
};
// The 2D viewport (x, y, width, height in the render target's pixels) for `fitted`: narrowed to the HUD's aspect
// inside a HudAspectScope when it is wider, else `fitted` itself.
void hud_viewport(const u32* fitted, u32* output);
// The platform, before the graphics device starts: applies the requested aspect for a width x height back buffer.
// Needs the widescreen video setting, as retail's widescreen code paths do. Returns the presented frame's aspect,
// or 0 when the frame keeps retail's 4:3 or 16:9.
float apply_wide_aspect(int width, int height, bool widescreen);

} // namespace gbtg
#endif
