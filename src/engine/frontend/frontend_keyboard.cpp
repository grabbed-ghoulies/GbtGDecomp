#include "gbtg/bundle.h"
#include "../audio/native_audio.h"
#include "gbtg/anim.h"
#include "gbtg/resource_error.h"
#include "gbtg/engine_internal.h"
#include "gbtg/font.h"
#include "gbtg/gfx.h"
#include "gbtg/hash.h"
#include "gbtg/loctext.h"
#include "gbtg/movie.h"
#include "gbtg/resource.h"
#include "gbtg/texture.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <map>
#include <string>
#include <tuple>
#include <vector>


#include "frontend_types.h"
#include "frontend.h"
#include "frontend_keyboard.h"
#include "frontend_slots.h"
#include "frontend_text.h"

namespace gbtg
{
using namespace frontend_detail;

namespace frontend_detail
{


const KbRow& kb_row(const Game& g)
{
    return kKb[g.kb_page][g.kb_row];
}


void kb_clamp_col(Game& g)
{
    const int n = kb_row(g).count;
    if (g.kb_col >= n)
    {
        g.kb_col = n - 1;
    }
    if (g.kb_col < 0)
    {
        g.kb_col = 0;
    }
}


void kb_open(Game& g)
{
    begin_naming_background(g);
    // 0x000E02D0 empty slot: object +0x94 = 4. 0x000DCEC0 loads
    // supportsavedgame__defaultnewgamename into the naming widget. 0x000110D0
    // copies that into the keyboard, and 0x00011870 stores its length at +0xA8,
    // which is the caret.
    g.frontend_mode = 4;
    g.naming = true;
    g.kb_page = 0;
    g.kb_row = 0;
    g.kb_col = 0;
    g.kb_saved = 1;
    g.kb_shift = false;
    g.kb_caps = false;
    const std::u16string name = default_new_game_name(g);
    g.kb_len = int(name.size());
    if (g.kb_len > 64)
    {
        g.kb_len = 64;
    }
    g.kb_caret = g.kb_len;
    // 0x00015A10 and 0x00016EF0 build new text widgets here, so the morph restarts.
    ++g.keyboard_generation;
    for (int i = 0; i < 64; ++i)
    {
        g.kb_text[i] = i < g.kb_len ? name[size_t(i)] : 0;
    }
}


bool kb_blocked(const Game& g)
{
    // FUN_00017560. Symbols page cannot rest on Shift or Caps Lock.
    const KbRow& row = kb_row(g);
    if (g.kb_col < 0 || g.kb_col >= row.count)
    {
        return true;
    }
    const u32 code = row.keys[g.kb_col].code;
    return g.kb_page == 1 && (code == 0x10002 || code == 0x10003);
}


void kb_up_adjust(Game& g)
{
    // FUN_00016950, jump table 0x16B14. Adjust the column, then the caller steps the row.
    const int page = g.kb_page;
    const int col = g.kb_col;
    switch (g.kb_row)
    {
    case 0:
        if (page == 1)
        {
            if (col > 1 && col < 5)
            {
                g.kb_saved = col;
                g.kb_col = 1;
            }
            else if (col == 5)
            {
                g.kb_col = (g.kb_saved == 2 || g.kb_saved == 3) ? g.kb_saved : 1;
                g.kb_saved = 5;
            }
        }
        else if (col > 1 && col < 7)
        {
            g.kb_saved = col;
            g.kb_col = 1;
        }
        else if (col > 6 && col < 9)
        {
            g.kb_saved = col;
            g.kb_col = 2;
        }
        else if (col > 8)
        {
            g.kb_saved = col;
            g.kb_col = 3;
        }
        break;
    case 1:
        if (page == 2 && col == 11)
        {
            g.kb_saved = 11;
            g.kb_col = 10;
        }
        else if (page == 1 && col == 6)
        {
            g.kb_saved = 6;
            g.kb_col = 5;
        }
        break;
    case 3:
        if (col == 7)
        {
            int saved = g.kb_saved;
            if (page == 1)
            {
                if (saved > 6)
                {
                    saved = 6;
                }
            }
            else if (saved < 7)
            {
                saved = 7;
            }
            g.kb_col = saved;
        }
        break;
    case 4:
        if (col == 1)
        {
            int saved = g.kb_saved;
            if (saved > 6)
            {
                saved = 6;
            }
            g.kb_col = saved;
        }
        else if (col > 1)
        {
            g.kb_col = 7;
        }
        break;
    default:
        break;
    }
}


void kb_down_adjust(Game& g)
{
    // FUN_00016B30, jump table 0x16D2C.
    const int page = g.kb_page;
    const int col = g.kb_col;
    switch (g.kb_row)
    {
    case 0:
        if (page == 2 && col == 10)
        {
            g.kb_col = g.kb_saved > 10 ? g.kb_saved : 10;
        }
        else if (page == 1 && col == 5)
        {
            int saved = g.kb_saved < 5 ? 5 : g.kb_saved;
            g.kb_col = saved > 6 ? 6 : saved;
        }
        break;
    case 2:
        if (col > 7)
        {
            g.kb_saved = col;
            g.kb_col = 7;
        }
        break;
    case 3:
        if (col > 0 && col < 7)
        {
            g.kb_saved = col;
            g.kb_col = 1;
        }
        else if (col >= 7)
        {
            g.kb_col = g.kb_saved > 8 ? 3 : 2;
        }
        break;
    case 4:
        if (col == 1)
        {
            int saved = g.kb_saved;
            const int cap = page == 1 ? 5 : 6;
            if (saved > cap)
            {
                saved = cap;
            }
            g.kb_col = saved;
        }
        else if (col == 2)
        {
            if (page == 1)
            {
                g.kb_saved = 2;
                g.kb_col = 5;
            }
            else
            {
                int saved = g.kb_saved;
                if (saved > 8)
                {
                    saved = 8;
                }
                else if (saved <= 7)
                {
                    saved = 7;
                }
                g.kb_col = saved;
            }
        }
        else if (col == 3)
        {
            if (page == 1)
            {
                g.kb_saved = 3;
                g.kb_col = 5;
            }
            else
            {
                g.kb_col = g.kb_saved < 10 ? 9 : 10;
            }
        }
        break;
    default:
        break;
    }
}


void kb_move(Game& g, int dcol, int drow)
{
    if (dcol != 0)
    {
        // 0x00016D40 / 0x00016DA0 wrap inside the row, then 0x00017560 may repeat the step.
        for (int guard = 0; guard < 16; ++guard)
        {
            const int n = kb_row(g).count;
            g.kb_col = (g.kb_col + dcol + n) % n;
            if (!kb_blocked(g))
            {
                return;
            }
        }
        return;
    }
    // +0xF0 is 5. A blocked landing (Shift / Caps on the symbols page) repeats the move.
    for (int guard = 0; guard < 16; ++guard)
    {
        if (drow < 0)
        {
            kb_up_adjust(g);
            g.kb_row = g.kb_row == 0 ? kKbRows - 1 : g.kb_row - 1;
        }
        else
        {
            kb_down_adjust(g);
            g.kb_row = g.kb_row == kKbRows - 1 ? 0 : g.kb_row + 1;
        }
        if (!kb_blocked(g))
        {
            return;
        }
    }
}


// 0x000357D0. EAX and EDX are the two names. 1 when both start with 'aid_' and
// the rest matches after AND 0xDF, or when neither starts with 'aid_'.
bool aid_name_match(std::string_view a, std::string_view b)
{
    auto aid_head = [](std::string_view s)
    {
        return s.size() >= 4 &&
            static_cast<unsigned char>(s[0]) == 'a' &&
            static_cast<unsigned char>(s[1]) == 'i' &&
            static_cast<unsigned char>(s[2]) == 'd' &&
            static_cast<unsigned char>(s[3]) == '_';
    };
    const bool a_aid = aid_head(a);
    const bool b_aid = aid_head(b);
    if (!a_aid)
    {
        return !b_aid;
    }
    if (!b_aid)
    {
        return false;
    }
    size_t i = 4;
    for (;;)
    {
        const unsigned char ca = (i < a.size() ? static_cast<unsigned char>(a[i]) : 0) & 0xDF;
        const unsigned char cb = (i < b.size() ? static_cast<unsigned char>(b[i]) : 0) & 0xDF;
        if (ca != cb)
        {
            return false;
        }
        if (ca == 0)
        {
            return true;
        }
        ++i;
    }
}


bool aid_slot(std::string_view name)
{
    return name.size() >= 4 &&
        static_cast<unsigned char>(name[0]) == 'a' &&
        static_cast<unsigned char>(name[1]) == 'i' &&
        static_cast<unsigned char>(name[2]) == 'd' &&
        static_cast<unsigned char>(name[3]) == '_';
}


// 0x00035960. The list is the packed aid names at *DAT_00511B64 (sceneorder).
// EBX is the key. The stack flag is 1 from 0x00010E9D0. A match whose next
// slot starts with 'aid_' returns that next name. The je at 0x000359BA lands
// on the epilogue, so the lea of the next slot is the return value.
const char* sceneorder_lookup(const Game& g, const char* key, int flag)
{
    if (g.scene_order.empty())
    {
        return nullptr;
    }
    if (key == nullptr)
    {
        return g.scene_order[0].c_str();
    }
    size_t i = 0;
    for (;;)
    {
        const bool next_aid = i + 1 < g.scene_order.size() && aid_slot(g.scene_order[i + 1]);
        if (aid_name_match(g.scene_order[i], key))
        {
            if (next_aid)
            {
                return g.scene_order[i + 1].c_str();
            }
            if (flag == 0)
            {
                return nullptr;
            }
            return g.scene_order[0].c_str();
        }
        if (!next_aid)
        {
            return nullptr;
        }
        ++i;
    }
}


// 0x00010E9D0. The new save's script id is the 0x00035960 result for
// aid_script_ghoulies_gameselect_gameselect (0x00322B8C).
void store_new_game_script(Game& g)
{
    const char* script = sceneorder_lookup(g, "aid_script_ghoulies_gameselect_gameselect", 1);
    char* dest = g.slot_script[g.slot];
    if (script == nullptr)
    {
        dest[0] = '\0';
        return;
    }
    size_t i = 0;
    while (script[i] != '\0' && i + 1 < sizeof(g.slot_script[g.slot]))
    {
        dest[i] = script[i];
        ++i;
    }
    dest[i] = '\0';
}


bool wide_name_equal(const char16_t* a, const char16_t* b)
{
    // 0x0014A91C. lstrcmpW.
    for (;;)
    {
        if (*a != *b)
        {
            return false;
        }
        if (*a == 0)
        {
            return true;
        }
        ++a;
        ++b;
    }
}


// 0x0010DB10. Walk the save list for the wide name and copy blob+0x102.
// A miss leaves the caller's buffer unwritten.
const char* save_script_for_slot(const Game& g, int slot)
{
    if (g.slot_name[slot][0] == 0)
    {
        return nullptr;
    }
    for (int i = 0; i < 3; ++i)
    {
        if (g.slot_script[i][0] == '\0')
        {
            continue;
        }
        if (wide_name_equal(g.slot_name[i], g.slot_name[slot]))
        {
            return g.slot_script[i];
        }
    }
    return nullptr;
}


const char* save_script(const Game& g)
{
    return save_script_for_slot(g, g.slot);
}


char16_t kb_insert_char(const Game& g, u32 code)
{
    // Keys are stored as the uppercase / symbol code. Shift (cleared after one insert) or caps
    // keeps A-Z; otherwise letters go in lowercase. FUN_000176E0 does this case map.
    if (code >= 'A' && code <= 'Z' && !g.kb_shift && !g.kb_caps)
    {
        return char16_t(code + 32);
    }
    return char16_t(code);
}


// Xbox 0x00015260
void kb_activate_with_audio(Game& g)
{
    kb_activate(g);
    // The retail wrapper plays after the edit, including rejected edits and Done: a 2D event voice that stops
    // itself.
    auto& voices = native_audio_runtime().voices;
    const i16 index = allocate_audio_event_voice(0, voices);
    if (index != -1)
    {
        voices.voices[size_t(index)].stop = 1;
        prepare_audio_event_voice(index, "XACT_SOUNDBANK_GSUPERSCARYSTUF_RIGHTBUTTON", voices);
    }
}


void kb_activate(Game& g)
{
    const u32 code = kb_row(g).keys[g.kb_col].code;
    // 0x000152F0. Codes below 0x10000 (except the caret arrows) insert.
    if (code == 0x22B2)
    {
        if (g.kb_caret > 0)
        {
            --g.kb_caret;
        }
        return;
    }
    if (code == 0x22B3)
    {
        if (g.kb_caret < g.kb_len)
        {
            ++g.kb_caret;
        }
        return;
    }
    if (code < 0x10000)
    {
        if (g.kb_len >= 64)
        {
            return;
        }
        for (int i = g.kb_len; i > g.kb_caret; --i)
        {
            g.kb_text[i] = g.kb_text[i - 1];
        }
        g.kb_text[g.kb_caret] = kb_insert_char(g, code);
        ++g.kb_caret;
        ++g.kb_len;
        g.kb_text[g.kb_len] = 0;
        if (g.kb_shift)
        {
            g.kb_shift = false;
        }
        return;
    }
    if (code == 0x10000)
    {
        if (g.kb_caret <= 0)
        {
            return;
        }
        for (int i = g.kb_caret - 1; i < g.kb_len; ++i)
        {
            g.kb_text[i] = g.kb_text[i + 1];
        }
        --g.kb_caret;
        --g.kb_len;
        return;
    }
    if (code == 0x10002)
    {
        g.kb_shift = !g.kb_shift;
        return;
    }
    if (code == 0x10003)
    {
        g.kb_caps = !g.kb_caps;
        return;
    }
    // Page keys rewrite +0xBC. 0x10005 symbols, 0x10006 accents, 0x10004 alphabet.
    if (code == 0x10005)
    {
        g.kb_page = 1;
        kb_clamp_col(g);
        return;
    }
    if (code == 0x10006)
    {
        g.kb_page = 2;
        kb_clamp_col(g);
        return;
    }
    if (code == 0x10004)
    {
        g.kb_page = 0;
        kb_clamp_col(g);
        return;
    }
    if (code == 0x10007)
    {
        // Xbox 0x000DD270 returns to widget 1, then 0x000DD6C0 closes
        // the naming widget with mode 4. That is not a saved-slot status.
        g.frontend_mode = 1;
        g.naming = false;
        if (g.kb_len > 0)
        {
            g.slot_name_len[g.slot] = g.kb_len;
            for (int i = 0; i < 64; ++i)
            {
                g.slot_name[g.slot][i] = g.kb_text[i];
            }
            g.slot_status[g.slot] = 2;
            // 0x00010D430's success path calls 0x00010E9D0 before 0x000DD270.
            // 0x00010E9D0 stores 0x00035960's return at blob+0x102. The key is
            // aid_script_ghoulies_gameselect_gameselect (0x00322B8C). On the retail
            // sceneorder that return is the following aid, chapter1_scene0_1book.
            store_new_game_script(g);
            std::cout << "slot " << g.slot << " named (" << g.kb_len << " chars)\n";
        }
    }
}


std::u16string key_label(const Game& g, u32 code)
{
    // 0x00011150. The wide string is the loctext pointer at [0x00338DCC + offset].
    // 0x20 Space, 0x10000 Backspace, 0x10002 Shift, 0x10003 Capslock,
    // 0x10004 Alphabet, 0x10005 Symbols, 0x10006 Accents, 0x10007 Done.
    const char* key = nullptr;
    switch (code)
    {
    case 0x10007:
        key = "virtualkeyboard_Done";
        break;
    case 0x10006:
        key = "virtualkeyboard_Accents";
        break;
    case 0x10005:
        key = "virtualkeyboard_Symbols";
        break;
    case 0x10004:
        key = "virtualkeyboard_Alphabet";
        break;
    case 0x10003:
        key = "virtualkeyboard_Capslock";
        break;
    case 0x10002:
        key = "virtualkeyboard_Shift";
        break;
    case 0x10000:
        key = "virtualkeyboard_Backspace";
        break;
    case 0x20:
        key = "virtualkeyboard_Space";
        break;
    case 0x22B2:
    case 0x22B3:
        return {};
    default:
        return std::u16string(1, kb_insert_char(g, code));
    }
    return gameselect_string(g, key);
}

// 0x00016FC0. x,y are the text pen; the button sits 5px up and left (DAT_0032B184)
// and is textWidth + 10 by cell*0.65 + 10 (DAT_0032B10C). 0xFFFF0000 is not drawn.
void draw_key_button(float pen_x, float pen_y, float text_w, float em, u32 color)
{
    if (color == 0xFFFF0000u)
    {
        return;
    }
    const float bw = text_w + 10.f;
    const float bh = em + 10.f;
    const int iw = int(bw);
    const int ih = int(bh);
    gfx_sprite_submit(rounded_fill(iw, ih, color), pen_x - 5.f, pen_y - 5.f, float(iw), float(ih));
}


// 0x00017210 chooses the button colour before 0x00016FC0.
// Default DAT_0020AE6C, hot key DAT_0020AE68. Shift (+0x10) and Caps (+0xC)
// use the hot colour on pages 0 and 2. Page 1 draws both of those keys with
// DAT_0020AE78. The hot key wins after that test.
u32 key_fill(const Game& g, u32 code, bool hot)
{
    constexpr u32 kFill = 0x80069764u;
    constexpr u32 kHot = 0xC43EF7B6u;
    constexpr u32 kGray = 0x80808080u;
    u32 color = kFill;
    if (code == 0x10002 || code == 0x10003)
    {
        const bool latched = code == 0x10002 ? g.kb_shift : g.kb_caps;
        if (g.kb_page == 1)
        {
            color = kGray;
        }
        else if (latched && (g.kb_page == 0 || g.kb_page == 2))
        {
            color = kHot;
        }
    }
    if (hot)
    {
        color = kHot;
    }
    return color;
}


const RgbaImage* joypad_icon(const Game& g, int code)
{
    if (code < 0 || code >= 9)
    {
        return nullptr;
    }
    for (const auto& image : g.ui)
    {
        if (image.name == kJoypadIconNames[code])
        {
            return &image.image;
        }
    }
    return nullptr;
}


void draw_keyboard(const Game& g)
{
    if (!g.naming)
    {
        return;
    }
    const GloopyFont& font = gloopy_font(g);
    if (font.cell() <= 0)
    {
        return;
    }
    const Game::UiImage* caret = find_ui(g, "virtkb_caret");
    const Game::UiImage* left = find_ui(g, "virtkb_arrowleft");
    const Game::UiImage* right = find_ui(g, "virtkb_arrowright");
    const int frame = font.rest_frame();
    constexpr float kLabel = 0.65f;
    constexpr float kName = 1.45f;
    const float em = float(font.cell()) * kLabel;
    const float name_em = float(font.cell()) * kName;

    // 0x00017210. Bar colour DAT_0020AE70. x=122, y=46. Width is
    // [keyboard+0x114] + 16, and +0x114 is screen width - 260. Height is
    // cell*1.45 + 16. Both extents are truncated before the fill.
    const int bar_w = int((kFrameW - 260.f) + 16.f);
    const int bar_h = int(name_em + 16.f);
    gfx_sprite_submit(rounded_fill(bar_w, bar_h, 0x40069764u), 122.f, 46.f, float(bar_w), float(bar_h));

    // 0x00015A10. Pen (130, 54), scale 1.45, eax=1. 0x00060070 morphs, then holds
    // the last frame. Insert calls 0x00015A10 again, which starts a new widget.
    // 0x000174D0 / 0x00015010 centres virtkb_caret on that string at the same scale.
    const std::u16string name(g.kb_text, g.kb_text + g.kb_len);
    float name_x = 130.f;
    float caret_x = 130.f;
    for (int i = 0; i < g.kb_len; ++i)
    {
        if (i == g.kb_caret)
        {
            caret_x = name_x;
        }
        const int name_frame = keyboard_name_frame(g, name, size_t(i));
        if (const RgbaImage* glyph = font.image(g.kb_text[i], name_frame))
        {
            const float bx = float(font.bearing(g.kb_text[i])) * kName;
            gfx_sprite_submit(*glyph, name_x - bx, 54.f, name_em, name_em);
        }
        name_x += float(font.advance(g.kb_text[i])) * kName;
    }
    if (g.kb_caret >= g.kb_len)
    {
        caret_x = name_x;
    }
    if (caret != nullptr)
    {
        const float cw = float(caret->image.width) * kName;
        const float ch = float(caret->image.height) * kName;
        const float cy = 54.f + name_em * 0.5f;
        draw_sprite(caret, caret_x - cw * 0.5f, cy - ch * 0.5f, cw, ch);
    }

    for (int r = 0; r < kKbRows; ++r)
    {
        const KbRow& row = kKb[g.kb_page][r];
        float x = 0.f;
        for (int c = 0; c < row.count; ++c)
        {
            const float slot = float(row.keys[c].width);
            const float pen_x = 64.f + x;
            const float pen_y = 135.f + float(r) * 42.f;
            const u32 code = row.keys[c].code;
            const bool hot = r == g.kb_row && c == g.kb_col;
            const std::u16string label = key_label(g, code);
            // 0x00016360 falls through to the key code. A missing glyph, which
            // is what the arrow codes are, advances by the cell, so the button
            // is square. Letters and words use the measured string.
            const float measured = label.empty() ? float(font.advance(code)) * kLabel : text_width(font, label, kLabel);
            draw_key_button(pen_x, pen_y, measured, em, key_fill(g, code, hot));
            if (!label.empty())
            {
                // 0x00016EF0 submits eax=1, so 0x00060070 plays the morph and
                // then holds the last frame. The highlight does not put it back
                // on the texture clock.
                float gx = pen_x;
                for (size_t i = 0; i < label.size(); ++i)
                {
                    const char16_t ch = label[i];
                    const int glyph_frame = keyboard_glyph_frame(g, g.kb_page, r, c, label, i);
                    if (const RgbaImage* glyph = font.image(ch, glyph_frame))
                    {
                        const float bx = float(font.bearing(ch)) * kLabel;
                        gfx_sprite_submit(*glyph, gx - bx, pen_y, em, em);
                    }
                    gx += float(font.advance(ch)) * kLabel;
                }
            }
            // 0x00015A60 / 0x00015B50. Centre is half the measured width across
            // and half an em down. 0x0005CA60 then centres the texture, scale 1.
            if (code == 0x22B2 || code == 0x22B3)
            {
                const Game::UiImage* arrow = code == 0x22B2 ? left : right;
                if (arrow != nullptr)
                {
                    const float aw = float(arrow->image.width);
                    const float ah = float(arrow->image.height);
                    const float cx = pen_x + measured * 0.5f;
                    const float cy = pen_y + em * 0.5f;
                    draw_sprite(arrow, cx - aw * 0.5f, cy - ah * 0.5f, aw, ah);
                }
            }
            x += slot;
            if (c == 0)
            {
                x += 4.f;
            }
        }
    }

    // VirtualKeyboardMenu, built by 0x00014AB0. On 640x480 the anchor is
    // (DAT_00509740 + 100, DAT_0050974C - 16) = (160.5, 415.5). Rows step by
    // DAT_0032B620 (-28.8). 0x0001428F0's width is the widest string plus
    // cell*0.65 + 20, and that opens the second column.
    // Codes: 0 A select, 1 B exit, 2 X change mode, 7 d-pad move, 8 triggers caret.
    // Face buttons stay in the left column, d-pad and triggers in the right,
    // lowest code on the anchor and the rest stacked upward.
    struct Prompt
    {
        int code;
        const char* key;
    };
    constexpr Prompt kPrompts[] = {
        {0, "virtualkeyboard_select"},
        {7, "virtualkeyboard_move"},
        {1, "virtualkeyboard_exit"},
        {8, "virtualkeyboard_movecaret"},
        {2, "virtualkeyboard_changemode"},
    };
    constexpr float kPrompt = 0.65f;
    constexpr float kAnchorX = 160.5f;
    constexpr float kAnchorY = 415.5f;
    constexpr float kRowStep = -28.8f;
    float widest = 0.f;
    for (const Prompt& prompt : kPrompts)
    {
        widest = std::max(widest, text_width(font, gameselect_string(g, prompt.key), kPrompt));
    }
    const float column = widest + float(font.cell()) * kPrompt + 20.f;
    const float prompt_em = float(font.cell()) * kPrompt;
    int left_row = 0;
    int right_row = 0;
    // Stable order is by icon code, not the file order of the five records.
    int order[5] = {0, 1, 2, 3, 4};
    std::sort(order, order + 5, [&](int a, int b) { return kPrompts[a].code < kPrompts[b].code; });
    for (int index : order)
    {
        const Prompt& prompt = kPrompts[index];
        const std::u16string part = gameselect_string(g, prompt.key);
        if (part.empty())
        {
            continue;
        }
        const bool right = prompt.code >= 7;
        const int row = right ? right_row++ : left_row++;
        const float text_x = kAnchorX + (right ? column : 0.f);
        const float mid_y = kAnchorY + float(row) * kRowStep;
        const float text_y = mid_y - prompt_em * 0.5f;
        if (const RgbaImage* icon = joypad_icon(g, prompt.code))
        {
            // DAT_0032B264 (0.7) is stored on the prompt child at +0x18.
            constexpr float kIcon = 0.7f;
            const float iw = float(icon->width) * kIcon;
            const float ih = float(icon->height) * kIcon;
            gfx_sprite_submit(*icon, text_x - iw - 4.f, mid_y - ih * 0.5f, iw, ih);
        }
        draw_glyphs(font, part, text_x, text_y, kPrompt, frame);
    }
}

} // namespace frontend_detail

} // namespace gbtg
