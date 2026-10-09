#include "dialogue_draw_context.h"
#include "gbtg/anim.h"
#include "../audio/listener_settings.h"
#include <bit>

namespace gbtg
{
// Xbox 0x0005F190
float dialogue_glyph_wave(u16 code, float time, float speed, float height,
    RuntimeDialogueDrawContext& c)
{
    const float sine = static_cast<float>(retail_sin((static_cast<float>(code) + time) * speed));
    c.state.random = c.state.random * 0x19660D + 0x3C6EF35F;
    const float fraction = std::bit_cast<float>((c.state.random & 0x7FFFFF) | 0x3F800000) - 1.f;
    return ((height - (0.f - height)) * fraction + (0.f - height)) * sine;
}
// Xbox 0x000608D0
float dialogue_glyph_bearing(i32 font, u16 code, float scale, RuntimeDialogueDrawContext& c)
{
    const auto& f = *c.metrics.fonts[font];
    i32 bearing = 0;
    if (code != 0 && f.first <= code && code <= f.last)
    {
        const auto& glyph = f.glyphs[code - f.first];
        // The dump zero-extends this u16 before comparing against -1.
        if (glyph.texture != nullptr && static_cast<u32>(glyph.marker) != 0xFFFFFFFFu)
        {
            bearing = glyph.left;
        }
    }
    if (scale != 0.f)
    {
        return static_cast<float>(bearing) * scale;
    }
    return static_cast<float>(bearing) * c.metrics.scale_x;
}
// Xbox 0x0005E9F0
void draw_runtime_dialogue_glyph(u32 code, float& x, float& y, float speed,
    float height, float time, u32 frame, u16 previous, i32 shadow,
    RuntimeDialogueDrawContext& c)
{
    const i32 font = c.state.font;
    float scales[2]{c.metrics.scale_x, c.metrics.scale_y};
    if (scales[0] == 0.f && scales[1] == 0.f)
    {
        return;
    }
    const bool wave = speed != 0.f && height != 0.f;
    float wave_offset = 0.f;
    float kern_x = 0.f;
    float kern_y = 0.f;
    if (wave)
    {
        wave_offset = dialogue_glyph_wave(static_cast<u16>(code), time, height, speed, c);
        y += wave_offset;
    }
    const u32 color = c.state.color;
    if (shadow != 0)
    {
        float shadow_x = c.state.shadow_x + x;
        float shadow_y = y + c.state.shadow_y;
        c.state.color = c.state.shadow_color;
        draw_runtime_dialogue_glyph(code, shadow_x, shadow_y, 0.f, 0.f, 0.f,
            frame, previous, 0, c);
        c.state.color = color;
    }
    const auto& f = *c.metrics.fonts[font];
    const u16 current = static_cast<u16>(code);
    const u8* texture = c.fallback_texture;
    u32 selected = 0;
    if (f.first <= current && current <= f.last)
    {
        const auto& glyph = f.glyphs[current - f.first];
        if (glyph.texture != nullptr && static_cast<u32>(glyph.marker) != 0xFFFFFFFFu)
        {
            texture = glyph.texture;
            selected = code;
        }
    }
    c.state.color = color;
    apply_runtime_text_kerning(c.metrics.kerning[font], previous, static_cast<u16>(selected),
        scales, &kern_x, &kern_y);
    const float bearing = dialogue_glyph_bearing(font, static_cast<u16>(selected), 0.f, c);
    const float position[2]{(x - bearing) + c.state.half_width + kern_x,
        y + c.state.half_height + kern_y};
    const float angle = c.state.angle;
    const u32 draw_color = c.state.color;
    begin_dialogue_sprite_draw(c);
    draw_dialogue_texture(static_cast<i32>(frame), 0, const_cast<u8*>(texture), scales,
        nullptr, angle, draw_color, position, c);
    --c.override_count;
    if (c.override_count == 0)
    {
        c.fog_disabled = 0;
    }
    x = runtime_text_character_advance(c.metrics, font, previous, static_cast<u16>(selected), 0.f) + x;
    if (wave)
    {
        y -= wave_offset;
    }
}
// Xbox 0x0005ED80
void draw_runtime_dialogue_text(const char16_t* text, float& x, float& y,
    u32 frame, const u32* frames, float speed, float height, float time, i32 shadow,
    RuntimeDialogueDrawContext& c)
{
    const i32 font = c.state.font;
    const float start_x = x;
    float current_x = x;
    float current_y = y;
    u16 previous = 0;
    float wrap = c.state.wrap;
    if (wrap == 0.f)
    {
        wrap = c.dialogue.wrap_width;
    }
    while (*text != 0)
    {
        const u32 glyph_frame = frames != nullptr ? *frames : frame;
        float next_x = current_x;
        float next_y = current_y;
        if (*text == '\n')
        {
            next_y = static_cast<float>(c.metrics.fonts[font]->drop) * c.metrics.scale_y + current_y;
            next_x = start_x;
        }
        else if (*text == ' ')
        {
            const float space = runtime_text_space_advance(*c.metrics.fonts[font], 0.f, c.metrics.scale_x);
            const float word = runtime_text_word_advance(c.metrics, font, text, 0, 0.f);
            if (wrap < word + space + current_x)
            {
                next_y = static_cast<float>(c.metrics.fonts[font]->drop) * c.metrics.scale_y + current_y;
                next_x = start_x;
            }
            else
            {
                next_x = space + current_x;
            }
        }
        else
        {
            draw_runtime_dialogue_glyph(*text, next_x, next_y, speed, height,
                time, glyph_frame, previous, shadow, c);
            if (wrap < next_x - start_x)
            {
                next_y = static_cast<float>(c.metrics.fonts[font]->drop) * c.metrics.scale_y + next_y;
                next_x = start_x;
            }
        }
        previous = *text++;
        if (frames != nullptr)
        {
            ++frames;
        }
        current_x = next_x;
        current_y = next_y;
    }
    x = current_x;
    y = current_y;
}
// Xbox 0x0005A630
float dialogue_line_position(i32 alignment, i32 line, float origin,
    const DialoguePageLayout& page, RuntimeDialogueDrawContext& c)
{
    if (alignment == 0)
    {
        return origin;
    }
    if (alignment == 1)
    {
        float width = page.line_widths[line];
        if (c.state.fade != 1.f)
        {
            width *= c.state.fade;
        }
        return (c.state.width * .5f + origin) - width * .5f;
    }
    if (alignment == 2)
    {
        float width = page.line_widths[line];
        if (c.state.fade != 1.f)
        {
            width *= c.state.fade;
        }
        return (c.state.width + origin) - width;
    }
    return 0.f;
}
}
