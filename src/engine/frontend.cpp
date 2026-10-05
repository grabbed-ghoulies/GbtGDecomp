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

// One row of the slot table at 0x0042902C, stride 0x124.
// Init copies the center into the widget at +0x428 / +0x42C.
// 0x000DF9A0 case 6 writes the sprite scale at +0x434 / +0x438:
//   screen * rest_norm / texture_size
// and the hit box at +0x510 / +0x514: rest_norm * {640,480} + DAT_00428FBC (12).
// 0x000DE540 lerps the selected slot from that rest scale to the 0.25 hot scale.
// 0x0005CA60 then centers a quad of texture_size * scale on +0x428. The box is
// only the argument to 0x00061360.
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
    // 0x0005CA60. On-screen size is texture pixels * scale, and scale is
    // (norm * screen) / texture pixels, so the quad is norm * {640,480}
    // centered on the table center.
    const float w = nx * kFrameW;
    const float h = ny * kFrameH;
    draw_sprite(find_ui(g, texture), slot.center_x - w * 0.5f, slot.center_y - h * 0.5f, w, h);
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

void draw_contents(const Game& g);

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

float text_width(const GloopyFont& font, std::u16string_view text, float scale)
{
    float width = 0.f;
    for (char16_t ch : text)
    {
        width += float(font.advance(ch)) * scale;
    }
    return width;
}

float banner_delay(unsigned& seed);

// Xbox 0x00060070. 0x0005FC30 calls it once the glyph delay at +0x204 has
// counted down. remaining is the life copy at +0x400, which starts at the
// life 0x0005F5C0 stored and then counts down. sticky is widget+0x121C.
// Gameselect passes EAX = 1 into 0x0005F5C0, so that flag is set and the
// life is 60 (0x42700000) instead of the caller's 1.0. The pool step at
// +0x20 is DAT_0032B0F4 (0.5). Glyph frame count is texture byte +0x11.
int goo_glyph_frame(float remaining, float life, float step, int count, int rate, bool sticky)
{
    constexpr float kEps = 0.001f;
    if (count <= 1)
    {
        return 0;
    }
    if (!sticky && kEps > remaining)
    {
        return 0;
    }
    if (life == 0.f)
    {
        return count - 1;
    }
    if (!(step > 0.f))
    {
        step = rate > 0 ? float(count) / float(rate) : 0.f;
    }
    const auto wrapped = [count](float ticks) {
        const int q = static_cast<int>(ticks);
        int frame = count == 0 ? 0 : q % count;
        if (frame < 0)
        {
            frame += count;
        }
        return frame;
    };
    if (remaining < life - step)
    {
        if (step < remaining || sticky)
        {
            return count - 1;
        }
        if (step == 0.f)
        {
            return 0;
        }
        const float span = (1.f / step) * float(count);
        if (span == 0.f)
        {
            return 0;
        }
        return wrapped(span * remaining);
    }
    if (step == 0.f)
    {
        return 0;
    }
    const float span = (1.f / step) * float(count);
    if (span == 0.f)
    {
        return 0;
    }
    return wrapped((life - remaining) * span);
}

// One 0x0005F5C0 widget. Delays are the pool's DAT_0054DD7C sequence.
struct GooLine
{
    float born = -1.f;
    int slot = -2;
    std::u16string text;
    std::vector<float> delay;
};

unsigned& goo_seed()
{
    static unsigned seed = 0;
    return seed;
}

void arm_goo(GooLine& line, const std::u16string& text, float now, int slot)
{
    line.text = text;
    line.born = now;
    line.slot = slot;
    line.delay.assign(text.size(), 0.f);
    unsigned& seed = goo_seed();
    for (float& delay : line.delay)
    {
        delay = banner_delay(seed);
    }
}

int goo_frame(const GooLine& line, size_t index, float now, int count)
{
    constexpr float kLife = 60.f;
    constexpr float kStep = 0.5f;
    const float age = now - line.born;
    const float delay = index < line.delay.size() ? line.delay[index] : 0.f;
    if (age < delay)
    {
        return 0;
    }
    float remaining = kLife - (age - delay);
    if (remaining < 0.f)
    {
        remaining = 0.f;
    }
    return goo_glyph_frame(remaining, kLife, kStep, count, 25, true);
}

void draw_goo_line(const GloopyFont& font, const GooLine& line, float pen_x, float pen_y, float scale, float now,
                   bool center)
{
    if (line.text.empty() || font.cell() <= 0 || scale == 0.f)
    {
        return;
    }
    const int count = font.rest_frame() + 1;
    float x = pen_x;
    if (center)
    {
        x -= text_width(font, line.text, scale) * 0.5f;
    }
    const float em = float(font.cell()) * scale;
    for (size_t i = 0; i < line.text.size(); ++i)
    {
        const char16_t ch = line.text[i];
        const int frame = goo_frame(line, i, now, count);
        if (const RgbaImage* glyph = font.image(ch, frame))
        {
            const float bx = float(font.bearing(ch)) * scale;
            gfx_sprite_submit(*glyph, x - bx, pen_y, em, em);
        }
        x += float(font.advance(ch)) * scale;
    }
}

// Contents widget, mode-table index 3. 0x000E02D0 sets the scene object's +0x94
// to 3 when the chosen slot's status is 2, 3, or 4, and that index's draw is
// 0x000D9640. 0x000D8B10 walks three records of 0x104 from 0x00428D04 and the
// layout table at 0x00306994 (stride 0x1C): centers (130, 200), (320, 200),
// (510, 200), rest norm 0.16, hot norm 0.25. The background at +0xC88 is the
// highlighted row's texture, centered, scaled up to the frame. 0x000D9D10
// lerps only the highlighted plate toward the hot norm. The save name sits at
// screen center + (0, -185) = (320, 55), scale 1.8. The highlighted row's
// loctext is the wide string at slot+4, placed at (320, 278), scale 1.4.
// 0x000DA040's chapter line is the buffer at +0xA78, anchor (320, 330), scale 0.75.
void draw_contents(const Game& g)
{
    struct Plate
    {
        const char* texture;
        const char* label;
        float center_x;
    };
    constexpr Plate kPlates[3] = {
        {"gameselect_house", "gameslotcontents_playgame", 130.f},
        {"gameselect_zombieclapframe", "gameslotcontents_features", 320.f},
        {"gameselect_skellyscratchheadframe", "gameslotcontents_challenges", 510.f},
    };
    const int hot = g.contents_row >= 0 && g.contents_row < 3 ? g.contents_row : 0;
    const Plate& shown = kPlates[hot];
    draw_sprite(find_ui(g, shown.texture), 0.f, 0.f, kFrameW, kFrameH);
    for (int i = 0; i < 3; ++i)
    {
        if (i == hot)
        {
            continue;
        }
        const float w = 0.16f * kFrameW;
        const float h = 0.16f * kFrameH;
        draw_sprite(find_ui(g, kPlates[i].texture), kPlates[i].center_x - w * 0.5f, 200.f - h * 0.5f, w, h);
    }
    {
        const float w = 0.25f * kFrameW;
        const float h = 0.25f * kFrameH;
        draw_sprite(find_ui(g, shown.texture), shown.center_x - w * 0.5f, 200.f - h * 0.5f, w, h);
    }

    const GloopyFont& font = gloopy_font(g);
    std::u16string name;
    if (g.slot_name_len[g.slot] > 0)
    {
        name.assign(g.slot_name[g.slot], g.slot_name[g.slot] + g.slot_name_len[g.slot]);
    }
    const std::u16string label = gameselect_string(g, shown.label);
    const std::u16string chapter = gameselect_string(g, "gameslotcontents_chapter");
    static int last_row = -2;
    static GooLine name_line;
    static GooLine label_line;
    static GooLine chapter_line;
    if (name_line.slot != g.slot || name_line.text != name)
    {
        arm_goo(name_line, name, g.time, g.slot);
    }
    if (last_row != hot || label_line.text != label)
    {
        arm_goo(label_line, label, g.time, hot);
    }
    if (chapter_line.text != chapter)
    {
        arm_goo(chapter_line, chapter, g.time, -1);
    }
    last_row = hot;
    draw_goo_line(font, name_line, 320.f, 55.f, 1.8f, g.time, true);
    draw_goo_line(font, label_line, 320.f, 278.f, 1.4f, g.time, true);
    draw_goo_line(font, chapter_line, 320.f, 330.f, 0.75f, g.time, true);

    const char* prompt_keys[3] = {
        "gameslotcontents_moveprompt",
        "gameslotcontents_selectprompt",
        "gameslotcontents_backprompt",
    };
    static GooLine prompts[3];
    constexpr float kPromptScale = 0.75f;
    constexpr float kPromptGap = 32.f;
    float x = 76.5f;
    for (int i = 0; i < 3; ++i)
    {
        const std::u16string part = gameselect_string(g, prompt_keys[i]);
        if (prompts[i].text != part)
        {
            arm_goo(prompts[i], part, g.time, -1);
        }
        if (part.empty())
        {
            continue;
        }
        draw_goo_line(font, prompts[i], x, 415.5f, kPromptScale, g.time, false);
        x += text_width(font, part, kPromptScale) + kPromptGap;
    }
}

// 0x000DF9A0 while the widget mode is 6 places the selected slot's wide name
// through 0x0005F5C0. The anchor is the per-slot point init wrote at +0x418:
// screen center plus DAT_00428FA8 (0) and DAT_00428FAC (-185). Scale is
// DAT_00428FA4 (1.8). param_8 = 1, so the line is centered on that point.
// The title is the same call with DAT_00428FB0 (1) at center + (0, 50).
// The prompt bar's y is DAT_0050974C - 16. On 640x480, 0x000FC190 stores
// that edge at 431.5, and the three prompt strings use DAT_00429024 (0.75).
void draw_slot_select_copy(const Game& g)
{
    const GloopyFont& font = gloopy_font(g);
    std::u16string name;
    if (g.slot_name_len[g.slot] > 0)
    {
        name.assign(g.slot_name[g.slot], g.slot_name[g.slot] + g.slot_name_len[g.slot]);
    }
    else
    {
        name = gameselect_string(g, "emptyslotname");
    }

    // 0x000DF9A0 builds these widgets again on the way into mode 6, including
    // each time the slot changes. The title and the prompt bar stay put.
    static int last_mode = -2;
    const bool entered = last_mode != 1;
    last_mode = g.frontend_mode;

    static GooLine slot_name;
    static GooLine title;
    static GooLine prompts[3];
    if (entered || slot_name.slot != g.slot || slot_name.text != name)
    {
        arm_goo(slot_name, name, g.time, g.slot);
    }
    const std::u16string title_text = gameselect_string(g, "gameslotselection_instructions");
    if (entered || title.text != title_text)
    {
        arm_goo(title, title_text, g.time, -1);
    }
    const char* prompt_keys[3] = {
        "gameslotselection_moveprompt",
        "gameslotselection_selectprompt",
        "gameslotselection_backtoattract",
    };
    for (int i = 0; i < 3; ++i)
    {
        const std::u16string part = gameselect_string(g, prompt_keys[i]);
        if (entered || prompts[i].text != part)
        {
            arm_goo(prompts[i], part, g.time, -1);
        }
    }

    draw_goo_line(font, slot_name, 320.f, 55.f, 1.8f, g.time, true);
    draw_goo_line(font, title, 320.f, 290.f, 1.f, g.time, true);

    constexpr float kPromptScale = 0.75f;
    constexpr float kPromptGap = 32.f;
    float x = 76.5f;
    for (const GooLine& part : prompts)
    {
        if (part.text.empty())
        {
            continue;
        }
        draw_goo_line(font, part, x, 415.5f, kPromptScale, g.time, false);
        x += text_width(font, part.text, kPromptScale) + kPromptGap;
    }
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

// 0x000E5B40, from 0x000DF9A0 case 1 when the slot widget opens. EAX is this
// cue and the id on the stack is 1. 0x00038910 acquires it; 0x000382C0 then
// binds a channel and 0x00038DA0 sets that voice playing. The cue-list row is
// Gstreamsditties / pause: device 3 (Gdvd8), wave 0.
constexpr const char* kSlotOpen = "XACT_SOUNDBANK_GSTREAMSDITTIES_PAUSE";

void begin_frontend(Game& g)
{
    // entitySceneControlAttract (registrar 0x00080970). The banner is that object,
    // built because the script names aid_objparams_ghoulies_scenecontrol_attract*.
    if (scene_control_starts(g, "attract"))
    {
        pcm_stop_cue(kSlotOpen);
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
        pcm_stop_cue(kSlotOpen);
        g.frontend_mode = -1;
        g.naming = false;
        return;
    }
    g.frontend_pending = 1;
    g.frontend_mode = g.frontend_pending;
    g.naming = false;
    pcm_play_cue(g, kSlotOpen, true);
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
            // 0x00322B8C. 0x0011EB30 queues it. The next 0x0011E4C0 that enters state 4 loads it.
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
