#pragma once

#include "frontend_types.h"

namespace gbtg
{
namespace frontend_detail
{
void kb_clamp_col(Game& g);

void kb_open(Game& g);

void kb_move(Game& g, int dcol, int drow);

// 0x0010DB10. Walk the save list for the wide name and copy blob+0x102.
// A miss leaves the caller's buffer unwritten.
const char* save_script_for_slot(const Game& g, int slot);

const char* save_script(const Game& g);

void kb_activate(Game& g);

void kb_activate_with_audio(Game& g); // Xbox 0x00015260

std::u16string key_label(const Game& g, u32 code);

const RgbaImage* joypad_icon(const Game& g, int code);

void draw_keyboard(const Game& g);

} // namespace frontend_detail
} // namespace gbtg
