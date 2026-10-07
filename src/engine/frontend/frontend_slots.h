#pragma once

#include "frontend_types.h"

namespace gbtg
{
namespace frontend_detail
{
// Xbox 0x0004B4D0. Suffixes at 0x004232F4 map through 0x002FE698.
int script_chapter_index(std::string_view script); // Xbox 0x0004B4D0

int slot_chapter(const Game& g, int index);

// Xbox 0x00101730: interpolate each packed color byte, clamp, then truncate.
u32 menu_color(u32 selected, u32 rest, float fraction); // Xbox 0x00101730

std::string slot_backdrop(const Game& g);

// Xbox 0x000DF140 / 0x000DA1E0: retain the previous background and start
// the countdown from button-bat +0x93C. The menu constructors override it.
void begin_menu_zoom(Game::MenuVisual& visual, const std::string& backdrop); // Xbox 0x000DF140

// Xbox 0x000DDF20 / 0x000D9640: current backdrop precedes the old backdrop.
void draw_menu_backdrop(const Game& g, const Game::MenuVisual& visual); // Xbox 0x000DDF20

// Xbox 0x000DDF20: plate pass, before the selected movie overlay.
void draw_plate(const Game& g, int index, bool selected); // Xbox 0x000DDF20

// Xbox 0x000DDF20: +0xB84 is tested against DAT_00429000 (10), then
// faded over DAT_00429004 frames. Static plates remain underneath the movie.
void draw_slot_movie(const Game& g); // Xbox 0x000DDF20

// Xbox 0x000DEB20 initializes the overlays. 0x000DE540 scales them with
// the selected plate, and 0x000DDF20 draws them after the movie overlay.
void draw_slot_status(const Game& g, int index); // Xbox 0x000DEB20

// Xbox 0x001421C0 / 0x00142720 / 0x00142AF0: mode 1 alternates
// columns, with icon centres and a separate offset for the text widgets.
void draw_menu_prompts(const Game& g, const GloopyFont& font,
                       const GooLine* prompts, const int* codes, int count,
                       float anchor_x, float anchor_y, float icon_scale,
                       float text_scale, float text_offset_x, float text_offset_y,
                       float column); // Xbox 0x001421C0

// 0x000DF9A0 while the widget mode is 6 places the selected slot's wide name
// through 0x0005F5C0. The anchor is the per-slot point init wrote at +0x418:
// screen center plus DAT_00428FA8 (0) and DAT_00428FAC (-185). Scale is
// DAT_00428FA4 (1.8). param_8 = 1, so the line is centered on that point.
// The title is the same call with DAT_00428FB0 (1) at center + (0, 50).
// The prompt bar's y is DAT_0050974C - 16. On 640x480, 0x000FC190 stores
// that edge at 431.5, and the three prompt strings use DAT_00429024 (0.75).
void draw_slot_select_copy(const Game& g);

} // namespace frontend_detail
} // namespace gbtg
