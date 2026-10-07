#pragma once

#include "frontend_types.h"

namespace gbtg
{
namespace frontend_detail
{
const GloopyFont& gloopy_font(const Game& g);

// Font table slot 1. 0x000605A0(1) selects aid_font_localisation_dialog_english_dialogfont.
const GloopyFont& dialog_font(const Game& g);

// Xbox 0x0005E9F0
// 0x00051760 advances by x1-x0. 0x000608D0 shifts the cell left by x0 so texel x0
// sits on the pen. 0x0005CA60 centers the quad, and the half-cell bias here cancels
// that, leaving the top-left at (pen - x0*scale, pen_y). A zero scale returns.
void draw_glyphs(const GloopyFont& font, std::u16string_view text, float pen_x, float pen_y, float scale, int frame); // Xbox 0x0005E9F0

// Xbox 0x00061360. Host rasterization of its central quad and mirrored
// horizontal scanlines. Keyboard calls pass 0.25; menu plates pass 0.1.
const RgbaImage& rounded_fill(int w, int h, u32 color, float corner_fraction = 0.25f); // Xbox 0x00061360

void draw_plate_background(float center_x, float center_y, float width, float height, u32 color);

std::u16string utf8_to_u16(std::string_view utf8);

// aid_loctext_ghoulies_gameselect_gameselecttext. 0x000FF120 joins "gameselect__" + key.
std::u16string gameselect_string(const Game& g, std::string_view key);

// Xbox 0x000FF0A0, saved-game callers use the supportsavedgame namespace.
std::u16string savedgame_string(const Game& g, std::string_view key); // Xbox 0x000FF0A0

// Xbox 0x000DCFC0.
std::u16string default_new_game_name(const Game& g); // Xbox 0x000DCFC0

float text_width(const GloopyFont& font, std::u16string_view text, float scale);

// Xbox 0x0005F5C0, sticky menu-string initialization.
void arm_goo(GooLine& line, const std::u16string& text, float, int slot); // Xbox 0x0005F5C0

// Xbox 0x0005F970 / 0x0005F7E0: end a widget using the pool's +0x20
// step and its glyph ages at +0xA04, then clear the sticky flag.
void end_goo(GooLine& line); // Xbox 0x0005F970

// One widget per key, born when the keyboard opens (0x00016EF0) or when
// 0x00015D60 rebuilds a letter after shift or caps changes the label.
int keyboard_glyph_frame(const Game& g, int page, int row, int col, const std::u16string& label, size_t index);

// 0x00015A10 replaces the name widget. A caret move does not.
int keyboard_name_frame(const Game& g, const std::u16string& name, size_t index);

void draw_goo_line(const GloopyFont& font, const GooLine& line, float pen_x, float pen_y, float scale, const Game& g,
                   bool center);

} // namespace frontend_detail
} // namespace gbtg
