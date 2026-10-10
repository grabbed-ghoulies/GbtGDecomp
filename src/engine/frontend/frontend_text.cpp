#include "../camera/camera.h"
#include "gbtg/bundle.h"
#include "gbtg/anim.h"
#include "gbtg/language.h"
#include "gbtg/resource_error.h"
#include "gbtg/dialogue.h"
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
#include "frontend_attract.h"
#include "frontend_keyboard.h"
#include "frontend_text.h"

namespace gbtg
{
using namespace frontend_detail;

namespace frontend_detail
{


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


// Font table slot 1. 0x000605A0(1) selects aid_font_localisation_dialog_english_dialogfont.
const GloopyFont& dialog_font(const Game& g)
{
    static GloopyFont font;
    static bool loaded = false;
    if (!loaded)
    {
        loaded = true;
        const auto path = g.aids.game_dir / "font" / "02d54854";
        if (!font.load(path))
        {
            std::cerr << "dialog font failed " << path.string() << "\n";
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


// Xbox 0x00061360. Host rasterization of its central quad and mirrored
// horizontal scanlines. Keyboard calls pass 0.25; menu plates pass 0.1.
const RgbaImage& rounded_fill(int w, int h, u32 color, float corner_fraction)
{
    const int radius = int(std::floor(float(h) * corner_fraction));
    struct Key
    {
        int w;
        int h;
        int radius;
        u32 color;
        bool operator<(const Key& other) const
        {
            return std::tie(w, h, radius, color) < std::tie(other.w, other.h, other.radius, other.color);
        }
    };
    static std::map<Key, RgbaImage> images;
    const Key key{w, h, radius, color};
    auto found = images.find(key);
    if (found == images.end())
    {
        RgbaImage image;
        image.width = w;
        image.height = h;
        image.bgra.assign(size_t(w * h * 4), 0);
        for (int y = 0; y < h; ++y)
        {
            const int dy = y < radius ? radius - y : y > h - radius ? y - (h - radius) : 0;
            const float inset = float(radius) - std::sqrt(float(radius * radius - dy * dy));
            for (int x = int(std::ceil(inset)); float(x) < float(w) - inset; ++x)
            {
                const size_t offset = size_t((y * w + x) * 4);
                image.bgra[offset] = u8(color >> 16);
                image.bgra[offset + 1] = u8(color >> 8);
                image.bgra[offset + 2] = u8(color);
                image.bgra[offset + 3] = u8(color >> 24);
            }
        }
        found = images.emplace(key, std::move(image)).first;
    }
    return found->second;
}


void draw_plate_background(float center_x, float center_y, float width, float height, u32 color)
{
    const int w = int(width);
    const int h = int(height);
    // 0x000DDF20 truncates each dimension before integer division by two.
    gfx_sprite_submit(rounded_fill(w, h, color, 0.1f), float(int(center_x) - w / 2),
                      float(int(center_y) - h / 2), float(w), float(h));
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
        const auto path = g.aids.game_dir / "loctext" / language_folder(g_language) / "04d74e34";
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


// Xbox 0x000FF0A0, saved-game callers use the supportsavedgame namespace.
std::u16string savedgame_string(const Game& g, std::string_view key)
{
    static std::map<std::string, std::u16string> strings;
    static bool loaded = false;
    if (!loaded)
    {
        loaded = true;
        const auto path = g.aids.game_dir / "loctext" / language_folder(g_language) / "070611e5";
        if (std::filesystem::exists(path))
        {
            const LocTable parsed = parse_loctext(read_file(path), "supportsavedgame");
            for (const auto& [name, value] : parsed.keys_to_utf8)
            {
                strings.emplace(name, utf8_to_u16(value));
            }
        }
    }
    const auto found = strings.find("supportsavedgame__" + std::string(key));
    return found == strings.end() ? u"ERROR" : found->second;
}


// Xbox 0x000DCFC0.
std::u16string default_new_game_name(const Game& g)
{
    return savedgame_string(g, "defaultnewgamename");
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
    if (!sticky && remaining < 0.001f)
    {
        return 0;
    }
    if (life != 0.f)
    {
        if (step <= 0.f)
        {
            step = float(count) / float(rate);
        }
        if (life - step <= remaining)
        {
            const float span = (1.f / step) * float(count);
            if (span == 0.f)
            {
                return 0;
            }
            return (int((life - remaining) * span) % count) & 0xff;
        }
        if (remaining <= step && !sticky)
        {
            // Xbox 0x000602E0.
            const float span = (1.f / step) * float(count);
            if (span != 0.f)
            {
                return (int(span * remaining) % count) & 0xff;
            }
            return 0;
        }
    }
    return count - 1;
}


unsigned& goo_seed()
{
    static unsigned seed = 0;
    return seed;
}


// Xbox 0x0005F5C0, sticky menu-string initialization.
void arm_goo(GooLine& line, const std::u16string& text, float, int slot)
{
    line.text = text;
    line.slot = slot;
    line.glyphs.assign(text.size(), GooLine::Glyph{});
    line.draw_frame = ~u32(0);
    line.draw_age = 0.f;
    line.sticky = true;
    line.removed = false;
    unsigned& seed = goo_seed();
    for (GooLine::Glyph& glyph : line.glyphs)
    {
        glyph.delay = banner_delay(seed);
    }
}


// Xbox 0x0005F970 / 0x0005F7E0: end a widget using the pool's +0x20
// step and its glyph ages at +0xA04, then clear the sticky flag.
void end_goo(GooLine& line)
{
    for (GooLine::Glyph& glyph : line.glyphs)
    {
        const float duration = banner_delay(goo_seed()) + 0.5f;
        if (duration <= glyph.age)
        {
            if (duration <= glyph.remaining)
            {
                glyph.remaining = duration;
            }
        }
        else
        {
            glyph.remaining = glyph.age;
        }
    }
    line.sticky = false;
}


// Xbox 0x0005FC30. Delay and life are separate countdowns: crossing a delay
// boundary does not spend the excess time on life in the same draw.
void tick_goo(const GooLine& line, const GloopyFont& font, float dt, u32 frame)
{
    if (line.draw_frame == frame)
    {
        return;
    }
    line.draw_frame = frame;
    size_t expired = 0;
    for (size_t i = 0; i < line.glyphs.size(); ++i)
    {
        GooLine::Glyph& glyph = line.glyphs[i];
        if (glyph.delay == 0.f)
        {
            const int count = font.glyph_frame_count(line.text[i]);
            glyph.frame = count == 0 ? -1 :
                goo_glyph_frame(glyph.remaining, 60.f, 0.5f, count,
                                font.glyph_frame_rate(line.text[i]), line.sticky);
            if (0.f < glyph.remaining)
            {
                glyph.remaining -= dt;
                if (glyph.remaining <= 0.f)
                {
                    glyph.remaining = 0.f;
                }
            }
            glyph.age += dt;
        }
        else if (0.f < glyph.delay)
        {
            glyph.delay -= dt;
            if (glyph.delay <= 0.f)
            {
                glyph.delay = 0.f;
            }
        }
        if (glyph.remaining < 0.001f && !line.sticky)
        {
            ++expired;
            glyph.frame = 0;
        }
    }
    if (expired == line.glyphs.size())
    {
        line.removed = true;
        return;
    }
    line.draw_age += dt;
}


int goo_frame(const GooLine& line, size_t index, const Game& g)
{
    tick_goo(line, gloopy_font(g), scene_sim_step(g.clock.dt), g.frame);
    return line.glyphs[index].frame;
}


// One widget per key, born when the keyboard opens (0x00016EF0) or when
// 0x00015D60 rebuilds a letter after shift or caps changes the label.
int keyboard_glyph_frame(const Game& g, int page, int row, int col, const std::u16string& label, size_t index)
{
    auto& slots = g.keyboard_text;
    if (g.keyboard_text_generation != g.keyboard_generation)
    {
        g.keyboard_text_generation = g.keyboard_generation;
        for (int p = 0; p < kKbPages; ++p)
        {
            for (int r = 0; r < kKbRows; ++r)
            {
                for (int c = 0; c < kKb[p][r].count && c < 12; ++c)
                {
                    arm_goo(slots[p][r][c], key_label(g, kKb[p][r].keys[c].code), g.time, -1);
                }
            }
        }
    }
    if (page < 0 || page >= kKbPages || row < 0 || row >= kKbRows || col < 0 || col >= 12)
    {
        return 0;
    }
    GooLine& slot = slots[page][row][col];
    if (slot.text != label)
    {
        arm_goo(slot, label, g.time, -1);
    }
    return goo_frame(slot, index, g);
}


// 0x00015A10 replaces the name widget. A caret move does not.
int keyboard_name_frame(const Game& g, const std::u16string& name, size_t index)
{
    GooLine& line = g.keyboard_name;
    if (g.keyboard_name_generation != g.keyboard_generation || line.text != name)
    {
        arm_goo(line, name, g.time, -1);
        g.keyboard_name_generation = g.keyboard_generation;
    }
    return goo_frame(line, index, g);
}


void draw_goo_line(const GloopyFont& font, const GooLine& line, float pen_x, float pen_y, float scale, const Game& g,
                   bool center)
{
    tick_goo(line, font, scene_sim_step(g.clock.dt), g.frame);
    if (line.removed || font.cell() <= 0 || scale == 0.f)
    {
        return;
    }
    float x = pen_x;
    if (center)
    {
        x -= text_width(font, line.text, scale) * 0.5f;
    }
    const float em = float(font.cell()) * scale;
    for (size_t i = 0; i < line.text.size(); ++i)
    {
        const char16_t ch = line.text[i];
        const int frame = goo_frame(line, i, g);
        if (const RgbaImage* glyph = font.image(ch, frame))
        {
            const float bx = float(font.bearing(ch)) * scale;
            gfx_sprite_submit(*glyph, x - bx, pen_y, em, em);
        }
        x += float(font.advance(ch)) * scale;
    }
}

} // namespace frontend_detail

} // namespace gbtg
