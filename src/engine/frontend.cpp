#include "gbtg/engine_internal.hpp"
#include "gbtg/font.hpp"
#include "gbtg/gfx.hpp"
#include "gbtg/loctext.hpp"
#include "gbtg/resource.hpp"
#include "gbtg/texture.hpp"

#include <algorithm>
#include <bit>
#include <filesystem>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace gbtg
{
namespace
{

std::u16string gameselect_string(const Game& g, std::string_view key);

RgbaImage image_from_entry(const BundleEntry& entry)
{
    TextureHeader hdr;
    if (entry.cpu.size() < 28 || !parse_texture_header(entry.cpu, hdr))
    {
        throw Error("texture header");
    }
    std::span<const u8> gpu = entry.gpu;
    if (!entry.gpu_chunks.empty())
    {
        const auto& chunk = entry.gpu_chunks[0];
        if (size_t(chunk.offset) + chunk.size <= entry.gpu.size())
        {
            gpu = entry.gpu.subspan(chunk.offset, chunk.size);
        }
    }
    return decode_xbox_texture(hdr, gpu);
}

const Game::UiImage* find_ui(const Game& g, std::string_view key)
{
    for (const auto& image : g.ui)
    {
        if (image.name.find(key) != std::string::npos)
        {
            return &image;
        }
    }
    return nullptr;
}

bool scene_control_is(const Game& g, std::string_view suffix)
{
    // The scene script names one objparams. The type table at 0x0030DB50
    // registers that object: scenecontrol_gameselect is entitySceneControlGameSelect.
    constexpr std::string_view kPrefix = "aid_objparams_ghoulies_scenecontrol_";
    for (const auto& aid : g.scene_script.aids)
    {
        if (aid.starts_with(kPrefix) && aid.substr(kPrefix.size()) == suffix)
        {
            return true;
        }
    }
    return false;
}

bool scene_control_starts(const Game& g, std::string_view suffix)
{
    constexpr std::string_view kPrefix = "aid_objparams_ghoulies_scenecontrol_";
    for (const auto& aid : g.scene_script.aids)
    {
        if (aid.starts_with(kPrefix) && aid.substr(kPrefix.size()).starts_with(suffix))
        {
            return true;
        }
    }
    return false;
}

// Xbox 0x0005CA60
// Submits the quad 0x0005E380 pushes. x,y is the top-left in the 640x480 HUD.
void draw_sprite(const Game::UiImage* image, float x, float y, float w, float h)
{
    if (image != nullptr)
    {
        gfx_sprite_submit(image->image, x, y, w, h);
    }
}

// Keys built by FUN_00011870. Width is the dword stored at key+4.
// Pages are the three groups split by FUN_00017A40. Layout (FUN_00015C40):
// x starts at 0, y at 135, row pitch is +0xEC (42). The first key of a row then adds 4.
// Draw adds the 64px margin at DAT_0032B450.
struct KbKey
{
    u32 code;
    int width;
};

struct KbRow
{
    KbKey keys[12];
    int count;
};

constexpr int kKbPages = 3;
constexpr int kKbRows = 5;

constexpr KbRow kKb[kKbPages][kKbRows] = {
    {
        {{{0x10007, 166}, {'1', 34}, {'2', 34}, {'3', 34}, {'4', 34}, {'5', 34}, {'6', 34}, {'7', 34}, {'8', 34}, {'9', 34}, {'0', 34}}, 11},
        {{{0x10002, 166}, {'A', 34}, {'B', 34}, {'C', 34}, {'D', 34}, {'E', 34}, {'F', 34}, {'G', 34}, {'H', 34}, {'I', 34}, {'J', 34}}, 11},
        {{{0x10003, 166}, {'K', 34}, {'L', 34}, {'M', 34}, {'N', 34}, {'O', 34}, {'P', 34}, {'Q', 34}, {'R', 34}, {'S', 34}, {'T', 34}}, 11},
        {{{0x10005, 166}, {'U', 34}, {'V', 34}, {'W', 34}, {'X', 34}, {'Y', 34}, {'Z', 34}, {0x10000, 136}}, 8},
        {{{0x10006, 166}, {0x20, 204}, {0x22B2, 68}, {0x22B3, 68}}, 4},
    },
    {
        {{{0x10007, 166}, {'(', 34}, {')', 34}, {'%', 34}, {'\\', 34}, {'/', 34}}, 6},
        {{{0x10002, 166}, {'$', 34}, {0x00A3, 34}, {0x00A5, 34}, {':', 34}, {'\'', 34}, {'"', 34}}, 7},
        {{{0x10003, 166}, {'?', 34}, {'!', 34}, {0x00BF, 34}, {0x00A1, 34}, {'-', 34}, {'&', 34}}, 7},
        {{{0x10004, 166}, {'{', 34}, {'}', 34}, {0x00AB, 34}, {0x00BB, 34}, {',', 34}, {'.', 34}, {0x10000, 136}}, 8},
        {{{0x10006, 166}, {0x20, 204}, {0x22B2, 68}, {0x22B3, 68}}, 4},
    },
    {
        {{{0x10007, 166}, {'1', 34}, {'2', 34}, {'3', 34}, {'4', 34}, {'5', 34}, {'6', 34}, {'7', 34}, {'8', 34}, {'9', 34}, {'0', 34}}, 11},
        {{{0x10002, 166}, {0x00C0, 34}, {0x00C1, 34}, {0x00C2, 34}, {0x00C4, 34}, {0x00C3, 34}, {0x00C5, 34}, {0x00C6, 34}, {0x00C7, 34}, {0x00C8, 34}, {0x00C9, 34}, {0x00CA, 34}}, 12},
        {{{0x10003, 166}, {0x00CB, 34}, {0x00CC, 34}, {0x00CD, 34}, {0x00CE, 34}, {0x00CF, 34}, {0x00D1, 34}, {0x00D2, 34}, {0x00D3, 34}, {0x00D4, 34}, {0x00D5, 34}, {0x00D8, 34}}, 12},
        {{{0x10004, 166}, {0x00D6, 34}, {0x00DF, 34}, {0x00D9, 34}, {0x00DA, 34}, {0x00DB, 34}, {0x00DC, 34}, {0x10000, 136}}, 8},
        {{{0x10006, 166}, {0x20, 204}, {0x22B2, 68}, {0x22B3, 68}}, 4},
    },
};

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
    // 0x000E02D0 empty slot: object +0x94 = 4 (gameslotnaming).
    g.frontend_mode = 4;
    g.naming = true;
    g.kb_page = 0;
    g.kb_row = 0;
    g.kb_col = 0;
    g.kb_saved = 1;
    g.kb_shift = false;
    g.kb_caps = false;
    g.kb_len = g.slot_name_len[g.slot];
    g.kb_caret = g.kb_len;
    for (int i = 0; i < 64; ++i)
    {
        g.kb_text[i] = i < g.kb_len ? g.slot_name[g.slot][i] : 0;
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
        if (g.kb_len >= 63)
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
        // FUN_000dd210. A nonempty name takes the 0x000DD270 path: mode 1 and status 4
        // (0x000DD6C0). An empty name does not occupy the slot.
        g.frontend_mode = 1;
        g.naming = false;
        if (g.kb_len > 0)
        {
            g.slot_name_len[g.slot] = g.kb_len;
            for (int i = 0; i < 64; ++i)
            {
                g.slot_name[g.slot][i] = g.kb_text[i];
            }
            g.slot_status[g.slot] = 4;
            std::cout << "slot " << g.slot << " named (" << g.kb_len << " chars)\n";
        }
    }
}

// One row of the slot table in .data. Names start at VA 0x429048, stride 0x124.
// FUN_000de7b0 (t = 0) writes center (x, y) into the widget at +0x428 / +0x42C.
// FUN_000df9a0 case 6 and FUN_000de540 write the box at +0x510 / +0x514:
//   rest      = norm * {640,480} + DAT_00428FBC (12)
//   highlight = 0.25 * {640,480} + DAT_00428FC0 (20)
// The draw at 0x000DE070 places the quad at center - box/2. Scale * texture size is norm * {640,480}.
struct SlotPlate
{
    const char* texture;
    float center_x;
    float center_y;
    float rest_x;
    float rest_y;
    float hot_x;
    float hot_y;
};

constexpr float kFrameW = 640.f;
constexpr float kFrameH = 480.f;
constexpr float kRestBias = 12.f;
constexpr float kHotBias = 20.f;

constexpr SlotPlate kSlots[3] = {
    {"gameselect_oneframe", 130.f, 200.f, 0.16f, 0.16f, 0.25f, 0.25f},
    {"gameselect_twoframe", 320.f, 200.f, 0.16f, 0.16f, 0.25f, 0.25f},
    {"gameselect_threeframe", 510.f, 200.f, 0.16f, 0.16f, 0.25f, 0.25f},
};

void draw_plate(const Game& g, int index, bool selected)
{
    const SlotPlate& slot = kSlots[index];
    // 0x000DEB20. Empty uses the slot's oneframe/twoframe/threeframe.
    // A named slot (status 4) uses the chapter table at 0x0042939C. A new game is chapter 1.
    const char* texture = g.slot_status[index] == 4 ? "gameselect_chapter1" : slot.texture;
    const float nx = selected ? slot.hot_x : slot.rest_x;
    const float ny = selected ? slot.hot_y : slot.rest_y;
    const float bias = selected ? kHotBias : kRestBias;
    // 0x000DE070 truncates the box, then places the quad at center - box/2.
    // The drawn size stays norm * {640,480}. The bias only shifts that center.
    const int box_w = int(nx * kFrameW + bias);
    const int box_h = int(ny * kFrameH + bias);
    const float left = slot.center_x - float(box_w / 2);
    const float top = slot.center_y - float(box_h / 2);
    draw_sprite(find_ui(g, texture), left, top, nx * kFrameW, ny * kFrameH);
    if (!selected)
    {
        return;
    }
    // FUN_001439c0 hangs bigarrowup / bigarrowdown on the selected slot widget.
    const Game::UiImage* up = find_ui(g, "gameselect_bigarrowup");
    const Game::UiImage* down = find_ui(g, "gameselect_bigarrowdown");
    const float aw = up ? float(up->image.width) : 128.f;
    const float ah = up ? float(up->image.height) : 32.f;
    const float ax = slot.center_x - aw * 0.5f;
    draw_sprite(up, ax, top - ah, aw, ah);
    const float dh = down ? float(down->image.height) : ah;
    draw_sprite(down, ax, top + ny * kFrameH, aw, dh);
}

std::u16string key_label(const Game& g, u32 code)
{
    // FUN_00011150 writes the loctext key from the table at 0x338D78.
    // 0x10004 +0x28 Alphabet, 0x10005 +0x2C Symbols, 0x10006 +0x30 Accents, 0x10007 +0x34 Done.
    // Letters follow FUN_000176E0 (shift / caps).
    switch (code)
    {
    case 0x10007:
        return u"DONE";
    case 0x10006:
        return u"ACCENTS";
    case 0x10005:
        return u"SYMBOLS";
    case 0x10004:
        return u"ALPHABET";
    case 0x10003:
        return u"CAPS LOCK";
    case 0x10002:
        return u"SHIFT";
    case 0x10000:
        return u"BACKSPACE";
    case 0x20:
        return u"SPACE";
    case 0x22B2:
    case 0x22B3:
        return {};
    default:
        return std::u16string(1, kb_insert_char(g, code));
    }
}

const GloopyFont& gloopy_font(const Game& g)
{
    static GloopyFont font;
    static bool loaded = false;
    if (!loaded)
    {
        loaded = true;
        const auto path = g.aids.game_dir / "font" / "069563d4";
        if (!font.load(path))
        {
            std::cerr << "gloopy font failed " << path.string() << "\n";
        }
    }
    return font;
}

// Xbox 0x0005E9F0
// 0x00051760 advances by x1-x0. 0x000608D0 shifts the cell left by x0 so texel x0
// sits on the pen. 0x0005CA60 centers the quad, and the half-cell bias here cancels
// that, leaving the top-left at (pen - x0*scale, pen_y). A zero scale returns.
void draw_glyphs(const GloopyFont& font, std::u16string_view text, float pen_x, float pen_y, float scale, int frame)
{
    if (font.cell() <= 0 || scale == 0.f)
    {
        return;
    }
    const float em = float(font.cell()) * scale;
    float x = pen_x;
    for (char16_t ch : text)
    {
        if (const RgbaImage* glyph = font.image(ch, frame))
        {
            const float bx = float(font.bearing(ch)) * scale;
            gfx_sprite_submit(*glyph, x - bx, pen_y, em, em);
        }
        x += float(font.advance(ch)) * scale;
    }
}

void draw_centered_line(const GloopyFont& font, std::u16string_view text, float anchor_x, float pen_y, float scale,
                        int frame)
{
    if (text.empty() || font.cell() <= 0 || scale == 0.f)
    {
        return;
    }
    float width = 0.f;
    for (char16_t ch : text)
    {
        width += float(font.advance(ch)) * scale;
    }
    draw_glyphs(font, text, anchor_x - width * 0.5f, pen_y, scale, frame);
}

void draw_keyboard(const Game& g)
{
    if (!g.naming)
    {
        return;
    }
    const GloopyFont& font = gloopy_font(g);
    const Game::UiImage* caret = find_ui(g, "virtkb_caret");
    const Game::UiImage* left = find_ui(g, "virtkb_arrowleft");
    const Game::UiImage* right = find_ui(g, "virtkb_arrowright");
    for (int r = 0; r < kKbRows; ++r)
    {
        const KbRow& row = kKb[g.kb_page][r];
        float x = 0.f;
        for (int c = 0; c < row.count; ++c)
        {
            const float w = float(row.keys[c].width);
            const float left_px = 64.f + x;
            const float top = 135.f + float(r) * 42.f;
            const u32 code = row.keys[c].code;
            const bool hot = r == g.kb_row && c == g.kb_col;
            if (code == 0x22B2 && left)
            {
                draw_sprite(left, left_px, top, float(left->image.width), float(left->image.height));
            }
            else if (code == 0x22B3 && right)
            {
                draw_sprite(right, left_px, top, float(right->image.width), float(right->image.height));
            }
            else
            {
                // FUN_00016EF0 submits every label with eax=1. FUN_00060070 then
                // settles on tile_count-1, and FUN_0005CA60 pins that index into
                // texture+0x13 so FUN_00053550's clock stays off. The key at
                // +0xC0/+0xC4 is left on the clock (frame -1, byte +0x13 stays 0xFF).
                const int frame = hot ? font.frame(g.time) : font.rest_frame();
                // FUN_00016EF0 submits sx = sy = 0.65 (DAT_0032B1EC).
                draw_glyphs(font, key_label(g, code), left_px, top, 0.65f, frame);
            }
            if (hot && caret)
            {
                const float cw = std::min(w, float(caret->image.width));
                const float ch = 8.f;
                draw_sprite(caret, left_px + (w - cw) * 0.5f, top + 42.f - ch, cw, ch);
            }
            x += w;
            if (c == 0)
            {
                x += 4.f;
            }
        }
    }
    // 0x00015A10 submits the name with eax=1, so FUN_00060070 holds the last frame.
    // Pen is (130, 54), scale 1.45 (DAT_0032B538). 0x000174D0 sits the caret on that string.
    float name_x = 130.f;
    const int frame = font.rest_frame();
    const float name_scale = 1.45f;
    const float name_em = float(font.cell()) * name_scale;
    for (int i = 0; i < g.kb_len; ++i)
    {
        if (i == g.kb_caret && caret)
        {
            draw_sprite(caret, name_x, 54.f, float(caret->image.width) * 0.45f, name_em);
        }
        if (const RgbaImage* glyph = font.image(g.kb_text[i], frame))
        {
            const float bx = float(font.bearing(g.kb_text[i])) * name_scale;
            gfx_sprite_submit(*glyph, name_x - bx, 54.f, name_em, name_em);
        }
        name_x += float(font.advance(g.kb_text[i])) * name_scale;
    }
    if (g.kb_caret >= g.kb_len && caret)
    {
        draw_sprite(caret, name_x, 54.f, float(caret->image.width) * 0.45f, name_em);
    }
}

void draw_contents(const Game& g)
{
    // Chapter plate stays on the slot center from 0x00429048. The rows are the
    // gameslotcontents keys: playgame, features, challenges.
    draw_sprite(find_ui(g, "gameselect_emptyslotbackground"), 0.f, 0.f, kFrameW, kFrameH);
    draw_plate(g, g.slot, true);
    const GloopyFont& font = gloopy_font(g);
    const int frame = font.rest_frame();
    const float scale = 0.65f;
    // 0x000DA0B5 / the contents keys. Play, Features, Bonus Challenges.
    const std::u16string rows_text[3] = {
        gameselect_string(g, "gameslotcontents_playgame"),
        gameselect_string(g, "gameslotcontents_features"),
        gameselect_string(g, "gameslotcontents_challenges"),
    };
    const int rows = g.frontend_mode == 6 ? 1 : 3;
    const float top = 300.f;
    const float step = 36.f;
    const float pen_x = 48.f;
    for (int i = 0; i < rows; ++i)
    {
        const bool hot = g.frontend_mode == 6 || i == g.contents_row;
        const int glyph = hot ? font.frame(g.time) : frame;
        const float y = top + float(i) * step;
        if (hot)
        {
            const Game::UiImage* arrow = find_ui(g, "gameselect_bigarrowup");
            const float ah = arrow ? float(arrow->image.height) : 32.f;
            draw_sprite(arrow, pen_x - 36.f, y, ah, ah * 0.5f);
        }
        draw_glyphs(font, rows_text[i], pen_x, y, scale, glyph);
    }
    if (g.slot_name_len[g.slot] > 0)
    {
        draw_glyphs(font, std::u16string_view(g.slot_name[g.slot], size_t(g.slot_name_len[g.slot])), pen_x, 48.f,
                    scale, frame);
    }
}

std::u16string utf8_to_u16(std::string_view utf8)
{
    std::u16string out;
    for (size_t i = 0; i < utf8.size();)
    {
        const unsigned char c = static_cast<unsigned char>(utf8[i]);
        char32_t cp = c;
        size_t n = 1;
        if (c >= 0xF0 && i + 3 < utf8.size())
        {
            cp = (char32_t(c & 0x07) << 18) | (char32_t(utf8[i + 1] & 0x3F) << 12) |
                 (char32_t(utf8[i + 2] & 0x3F) << 6) | char32_t(utf8[i + 3] & 0x3F);
            n = 4;
        }
        else if (c >= 0xE0 && i + 2 < utf8.size())
        {
            cp = (char32_t(c & 0x0F) << 12) | (char32_t(utf8[i + 1] & 0x3F) << 6) | char32_t(utf8[i + 2] & 0x3F);
            n = 3;
        }
        else if (c >= 0xC0 && i + 1 < utf8.size())
        {
            cp = (char32_t(c & 0x1F) << 6) | char32_t(utf8[i + 1] & 0x3F);
            n = 2;
        }
        if (cp > 0xFFFF)
        {
            cp -= 0x10000;
            out.push_back(char16_t(0xD800 + (cp >> 10)));
            out.push_back(char16_t(0xDC00 + (cp & 0x3FF)));
        }
        else
        {
            out.push_back(char16_t(cp));
        }
        i += n;
    }
    return out;
}

// aid_loctext_ghoulies_gameselect_gameselecttext. 0x000FF120 joins "gameselect__" + key.
std::u16string gameselect_string(const Game& g, std::string_view key)
{
    static std::map<std::string, std::u16string> table;
    static bool loaded = false;
    if (!loaded)
    {
        loaded = true;
        const auto path = g.aids.game_dir / "loctext" / "english" / "04d74e34";
        if (std::filesystem::exists(path))
        {
            try
            {
                const LocTable parsed = parse_loctext(read_file(path), path.filename().string());
                for (const auto& [k, v] : parsed.keys_to_utf8)
                {
                    table.emplace(k, utf8_to_u16(v));
                }
            }
            catch (const std::exception& ex)
            {
                std::cerr << "gameselect loctext " << ex.what() << "\n";
            }
        }
    }
    std::string full = "gameselect__";
    full.append(key);
    const auto it = table.find(full);
    if (it == table.end())
    {
        return {};
    }
    return it->second;
}

// Xbox 0x000DFD70 writes gameslotselection_instructions ("Choose A Game").
// 0x000FC190 on 640x480 stores DAT_00509730 = 320 and DAT_00509734 = 240.
// The title anchor is that point plus DAT_00428FB4 (0) and DAT_00428FB8 (50).
// The same case places the prompt bar at DAT_0050974C - DAT_0032B1F4 (443.5 - 16).
void draw_slot_select_copy(const Game& g)
{
    const GloopyFont& font = gloopy_font(g);
    const int frame = font.rest_frame();
    constexpr float kScale = 1.f;
    draw_centered_line(font, gameselect_string(g, "gameslotselection_instructions"), 320.f, 290.f, kScale, frame);

    const std::u16string move = gameselect_string(g, "gameslotselection_moveprompt");
    const std::u16string select = gameselect_string(g, "gameslotselection_selectprompt");
    const std::u16string back = gameselect_string(g, "gameslotselection_backtoattract");
    std::u16string prompts;
    for (const std::u16string* part : {&move, &select, &back})
    {
        if (part->empty())
        {
            continue;
        }
        if (!prompts.empty())
        {
            prompts += u"    ";
        }
        prompts += *part;
    }
    draw_centered_line(font, prompts, 320.f, 427.5f, kScale, frame);
}

// Xbox 0x000FF120 builds "%s__%s" from the two cstrings at 0x00323EE4 / 0x00323EEC.
std::u16string attract_start_banner(const Game& g)
{
    static std::u16string text;
    static bool loaded = false;
    if (loaded)
    {
        return text;
    }
    loaded = true;
    // aid_loctext_ghoulies_attract_attracttext
    const auto path = g.aids.game_dir / "loctext" / "english" / "0bbbe8e4";
    if (!std::filesystem::exists(path))
    {
        return text;
    }
    try
    {
        const LocTable table = parse_loctext(read_file(path), path.filename().string());
        const auto it = table.keys_to_utf8.find("attract__StartBanner");
        if (it != table.keys_to_utf8.end())
        {
            text = utf8_to_u16(it->second);
        }
    }
    catch (const std::exception& ex)
    {
        std::cerr << "start banner loctext " << ex.what() << "\n";
    }
    return text;
}

// 0x0005F6E2. Delay is (rng_float in [1, 2) - 1) * DAT_0032B104.
float banner_delay(unsigned& seed)
{
    seed = seed * 0x19660Du + 0x3C6EF35Fu;
    const float unit = std::bit_cast<float>((seed & 0x7fffffu) | 0x3f800000u);
    return (unit - 1.f) * 0.25f;
}

// Xbox 0x00060070. The text pool's step at +0x20 is 0.5 (0x0005F40A, DAT_0032B0F4).
// Life is the 4.0 passed by 0x000809C0. edi (widget+0x121C) is 0, so a spent life
// returns frame 0. Otherwise one forward pass, the last frame, then one pass back.
int banner_glyph_frame(float age, float delay, int count)
{
    constexpr float kLife = 4.f;
    constexpr float kStep = 0.5f;
    constexpr float kEps = 0.001f;
    if (count <= 1)
    {
        return 0;
    }
    const float t = age - delay;
    if (t < 0.f)
    {
        return 0;
    }
    const float remaining = kLife - t;
    if (remaining <= kEps)
    {
        return 0;
    }
    if (remaining >= kLife - kStep)
    {
        return int(t * float(count) / kStep) % count;
    }
    if (kStep < remaining)
    {
        return count - 1;
    }
    return int(remaining * float(count) / kStep) % count;
}

// Xbox 0x00080A30 creates the attract banner. 0x000809C0 places the widget:
// x = DAT_00509730 (0x000FC190, width/2), y = DAT_0050972C - font[+0x10] * DAT_0032B5B4.
// param_8 = 1, so 0x0005EF40 draws from anchor_x - measured_width/2.
// 0x00080C10 counts DAT_0032B284 (7s) and builds the widget again.
void draw_start_banner(const Game& g)
{
    if (g.scene_name.find("attract") == std::string::npos)
    {
        return;
    }
    const std::u16string text = attract_start_banner(g);
    if (text.empty())
    {
        return;
    }
    const GloopyFont& font = gloopy_font(g);
    const float scale = 1.125f;
    const float inset = std::min(kFrameH * 0.85f, kFrameH * 0.90f);
    const float anchor_y = kFrameH - (kFrameH - inset) * 0.5f;
    const float pen_y = anchor_y - float(font.drop()) * scale;
    float width = 0.f;
    for (char16_t ch : text)
    {
        width += float(font.advance(ch)) * scale;
    }
    const float pen_x = kFrameW * 0.5f - width * 0.5f;

    constexpr float kPeriod = 7.f;
    struct BannerAnim
    {
        float born = -1.f;
        unsigned seed = 1;
        std::vector<float> delay;
    };
    static BannerAnim anim;
    if (anim.born < 0.f || g.time - anim.born >= kPeriod || anim.delay.size() != text.size())
    {
        anim.born = g.time;
        anim.delay.resize(text.size());
        for (float& delay : anim.delay)
        {
            delay = banner_delay(anim.seed);
        }
    }
    const float age = g.time - anim.born;
    const int count = font.rest_frame() + 1;
    const float em = float(font.cell()) * scale;
    float x = pen_x;
    for (size_t i = 0; i < text.size(); ++i)
    {
        const char16_t ch = text[i];
        const int frame = banner_glyph_frame(age, anim.delay[i], count);
        if (const RgbaImage* glyph = font.image(ch, frame))
        {
            const float bx = float(font.bearing(ch)) * scale;
            gfx_sprite_submit(*glyph, x - bx, pen_y, em, em);
        }
        x += float(font.advance(ch)) * scale;
    }
}

} // namespace

void load_ui(Game& g)
{
    g.ui.clear();
    // 0x000DD740 resolves these aids with 0x00035E90. They are files in the
    // scene bundle the gameselect scene control just opened, not a name test.
    if (!g.scene_bundle || !scene_control_is(g, "gameselect"))
    {
        return;
    }
    for (const auto* entry : entries_of_type(*g.scene_bundle, ResourceType::Texture))
    {
        try
        {
            Game::UiImage image;
            image.name = entry->name;
            image.image = image_from_entry(*entry);
            g.ui.push_back(std::move(image));
        }
        catch (const std::exception& ex)
        {
            std::cout << "  ui fail " << entry->name << ": " << ex.what() << "\n";
        }
    }
    std::cout << "  ui textures " << g.ui.size() << "\n";
}

void begin_frontend(Game& g)
{
    // entitySceneControlAttract (registrar 0x00080970). The banner is that object,
    // built because the script names aid_objparams_ghoulies_scenecontrol_attract*.
    if (scene_control_starts(g, "attract"))
    {
        g.frontend_mode = 0;
        g.naming = false;
        // A new attract script builds a new banner, so +0x294 starts clear.
        g.attract_banner_latched = false;
        std::cout << "title attract\n";
        return;
    }
    // entitySceneControlGameSelect init 0x00083660. It calls 0x000E5AE0, which
    // installs modes[DAT_00429BC0] when stream id 2 is idle (0x00039D50 returns 0).
    // Boot 0x000E5A80 stored 1, and 0x000DD740 stores 1 again: slot select.
    // A scene with no frontend scene-control leaves the widget down.
    if (!scene_control_is(g, "gameselect"))
    {
        g.frontend_mode = -1;
        g.naming = false;
        return;
    }
    g.frontend_pending = 1;
    g.frontend_mode = g.frontend_pending;
    g.naming = false;
    std::cout << "frontend mode 1 slot select\n";
}

void gameselect_tick(Game& g, const InputState& in)
{
    if (g.frontend_mode == 0)
    {
        g.naming = false;
        // 0x00080C10 calls 0x00080CB0 while the banner dword at +0x294 is 0.
        // 0x001431B0 accepts a new A or Start, not both. 0x0011EB30 then requests
        // the gameselect script copied from 0x00322B8C.
        if (!g.attract_banner_latched && attract_confirm_pressed())
        {
            g.attract_banner_latched = true;
            // 0x00322B8C. 0x0011EB30 queues it. frame_tick's 0x0011E4C0 loads it.
            script_request(g, "aid_script_ghoulies_gameselect_gameselect");
        }
    }
    else if (g.frontend_mode == 4)
    {
        // 0x00015090. B cancels (codes 3 and 4 call 0x000DD550). D-pad moves. A activates.
        if (in.back && !g.slot_back)
        {
            g.frontend_mode = 1;
            g.naming = false;
        }
        else
        {
            if (in.left && !g.slot_left)
            {
                kb_move(g, -1, 0);
            }
            if (in.right && !g.slot_right)
            {
                kb_move(g, 1, 0);
            }
            if (in.up && !g.slot_up)
            {
                kb_move(g, 0, -1);
            }
            if (in.down && !g.slot_down)
            {
                kb_move(g, 0, 1);
            }
            if (in.accept && !g.slot_accept)
            {
                kb_activate(g);
            }
        }
    }
    else if (g.frontend_mode == 1)
    {
        // 0x000E0210: direction 8 wraps the index down, direction 4 wraps it up. Both end in state 6.
        if (in.down && !g.slot_down)
        {
            g.slot = (g.slot + 1) % 3;
        }
        if (in.up && !g.slot_up)
        {
            g.slot = (g.slot + 2) % 3;
        }
        // 0x000E02D0: empty slot sets mode 4. Status 2, 3, or 4 sets mode 3 (slot contents).
        if (in.accept && !g.slot_accept)
        {
            if (g.slot_status[g.slot] == 0)
            {
                kb_open(g);
            }
            else
            {
                g.frontend_mode = 3;
                g.contents_row = 0;
                g.naming = false;
                std::cout << "frontend mode 3 slot contents\n";
            }
        }
        // 0x000E05D0 copies aid_script_ghoulies_gameselect_backtoattract from 0x00324F30
        // into 0x0050E458, then 0x0011EAD0 requests that name.
        if (in.back && !g.slot_back)
        {
            g.naming = false;
            script_request(g, "aid_script_ghoulies_gameselect_backtoattract");
        }
    }
    else if (g.frontend_mode == 3)
    {
        // 0x000D9450. State 1 accept (0x000D94FD) pushes 6 into 0x000DA580, the play row.
        // B leaves the contents widget for slot select.
        if (in.down && !g.slot_down)
        {
            g.contents_row = (g.contents_row + 1) % 3;
        }
        if (in.up && !g.slot_up)
        {
            g.contents_row = (g.contents_row + 2) % 3;
        }
        if (in.accept && !g.slot_accept)
        {
            if (g.contents_row == 0)
            {
                g.frontend_mode = 6;
                std::cout << "frontend mode 6 play\n";
            }
            else if (g.contents_row == 1)
            {
                g.slot_status[g.slot] = 0;
                g.slot_name_len[g.slot] = 0;
                g.frontend_mode = 1;
                std::cout << "slot " << g.slot << " deleted\n";
            }
        }
        if (in.back && !g.slot_back)
        {
            g.frontend_mode = 1;
        }
    }
    else if (g.frontend_mode == 6)
    {
        // Play row (0x000D6410 / 0x000D67F0). A starts the saved game at chapter 1.
        // B returns to the contents rows.
        if (in.back && !g.slot_back)
        {
            g.frontend_mode = 3;
        }
        else if (in.accept && !g.slot_accept)
        {
            // 0x000E8520 copies aid_script_ghoulies_chapter1_scene0_1book from 0x00325500.
            std::cout << "play\n";
            g.naming = false;
            script_request(g, "aid_script_ghoulies_chapter1_scene0_1book");
        }
    }
    g.slot_down = in.down;
    g.slot_up = in.up;
    g.slot_left = in.left;
    g.slot_right = in.right;
    g.slot_accept = in.accept;
    g.slot_back = in.back;
}

void draw_ui(const Game& g)
{
    gfx_sprite_clear();
    if (g.frontend_mode < 0)
    {
        return;
    }
    if (g.frontend_mode == 0)
    {
        draw_start_banner(g);
        return;
    }
    if (g.frontend_mode == 3 || g.frontend_mode == 6)
    {
        draw_contents(g);
        return;
    }
    // 0x000DF140: an empty slot's backdrop is emptyslotbackground, scaled to the 640x480 frame.
    draw_sprite(find_ui(g, "gameselect_emptyslotbackground"), 0.f, 0.f, kFrameW, kFrameH);
    for (int i = 0; i < 3; ++i)
    {
        if (i != g.slot)
        {
            draw_plate(g, i, false);
        }
    }
    draw_plate(g, g.slot, true);
    if (g.frontend_mode == 1)
    {
        draw_slot_select_copy(g);
    }
    draw_keyboard(g);
}

} // namespace gbtg
