#pragma once
#include "gameselect_rows.h"
#include "gbtg/entity.h"

#include <vector>

namespace gbtg
{
struct RuntimeDialogueTextPool;
struct RuntimeDialogueWidget;
// One key (0x10 bytes): its code (a character, or 0x10000.. for the function keys), width, label (the XBE address
// of a wide key name in the label table, or 0) and its label's text record.
struct VirtualKeyboardKey
{
    u32 code = 0; // +0
    u32 width = 0; // +4
    u32 label = 0; // +8
    u32 text = 0; // +0xC
};
using VirtualKeyboardRow = std::vector<VirtualKeyboardKey>;
using VirtualKeyboardBoard = std::vector<VirtualKeyboardRow>;
// The naming screen's virtual keyboard (300 bytes, 0x00011270, vtable DAT_00434118). Its boards (+0xAC) are
// vectors of rows of keys; the retail std::vector helpers (0x000177xx..0x000190xx) are the host vector.
struct VirtualKeyboard
{
    std::array<RuntimeDialogueTextPool*, 3> pools{}; // +0 key labels, +4, +8 the name
    u32 caps = 0; // +0xC
    u32 shift = 0; // +0x10
    u32 fresh = 0; // +0x14, the default name is replaced by the first key.
    u32 done = 0; // +0x18
    u32 language = 0; // +0x1C, the layout set (2 is the Japanese one).
    std::array<char16_t, 0x40> name{}; // +0x24
    u32 name_text = 0; // +0xA4
    i32 cursor = 0; // +0xA8
    std::vector<VirtualKeyboardBoard> boards; // +0xAC
    i32 board = 0; // +0xBC
    i32 row = 0; // +0xC0
    i32 column = 0; // +0xC4
    i32 kept_column = 0; // +0xC8
    float repeat = 0.f; // +0xCC
    std::span<const u8> caret; // +0xD0, aid_texture_ghoulies_gameselect_virtkb_caret
    std::span<const u8> arrow_left; // +0xD4
    std::span<const u8> arrow_right; // +0xD8
    u32 word_e0 = 0; // +0xE0
    u32 word_e4 = 0; // +0xE4
    u32 layout = 0; // +0xE8, the language's layout row (DAT_00434118 ..).
    float row_spacing = 0.f; // +0xEC
    u32 rows = 0; // +0xF0
    Actor* key_bat = nullptr; // +0xF4
    Actor* caret_bat = nullptr; // +0xF8
    ButtonBatPoint key_target{}; // +0xFC
    ButtonBatPoint caret_target{}; // +0x104
    RuntimeDialogueWidget* prompts = nullptr; // +0x10C
    std::span<u8> input; // +0x110
    u32 input_address = 0;
    float width_limit = 0.f; // +0x114
    u32 buttons = 0; // +0x118
    u32 word_11c = 0; // +0x11C
    u32 word_120 = 0; // +0x120
    u32 word_124 = 0; // +0x124
    u32 word_128 = 0; // +0x128
};
VirtualKeyboard* create_virtual_keyboard(const char16_t* name, GameSelectControlServices& services); // Xbox 0x000110D0
void tick_virtual_keyboard(VirtualKeyboard& keyboard, GameSelectControlServices& services); // Xbox 0x00011800, ESI
void draw_virtual_keyboard(VirtualKeyboard& keyboard, GameSelectControlServices& services); // Xbox 0x00011110, ESI
void release_virtual_keyboard(VirtualKeyboard& keyboard, GameSelectControlServices& services); // Xbox 0x000113C0
void destroy_virtual_keyboard(VirtualKeyboard*& keyboard); // Xbox 0x000663E0, EAX the keyboard
}
