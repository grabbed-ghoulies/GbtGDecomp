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
#include "frontend_slots.h"
#include "frontend_text.h"

namespace gbtg
{
using namespace frontend_detail;

namespace frontend_detail
{


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
    const auto path = g.aids.game_dir / "loctext" / language_folder(g_language) / "0bbbe8e4";
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
// param_8, so 0x0005EF40 draws from anchor_x - measured_width/2.
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

} // namespace frontend_detail

} // namespace gbtg
