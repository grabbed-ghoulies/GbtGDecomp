#include "dialogue_text_metrics.h"
#include "../runtime/crt_format.h"
#include <bit>
#include <cmath>

namespace gbtg
{
// Xbox 0x00060A40
i32 measure_runtime_text_block(const RuntimeTextMetrics& state, i32 font,
    const char16_t* text, float scale, float wrap_width, float viewport_width,
    float* dimensions)
{
    float maximum = 0.f;
    float current = 0.f;
    i32 lines = 1;
    const float space = runtime_text_space_advance(*state.fonts[font], scale, state.scale_x);
    if (wrap_width <= 0.f)
    {
        wrap_width = viewport_width;
    }
    u32 pending = std::bit_cast<u32>(scale); // EDI initially loads the scale argument's bits.
    while (*text != 0)
    {
        if (*text == u' ')
        {
            const float next = runtime_text_word_advance(state, font, text + 1, 0, scale);
            if (current != 0.f)
            {
                const float combined = (next + space) + current;
                if (combined > wrap_width)
                {
                    if (current > maximum)
                    {
                        maximum = current;
                    }
                    ++lines;
                    current = 0.f;
                    pending = 1;
                }
                else
                {
                    current = space + current;
                    pending = 0;
                }
            }
        }
        else
        {
            const float word = runtime_text_word_advance(state, font, text, 0, scale);
            const float combined = word + current;
            if (combined > wrap_width)
            {
                if (current > maximum)
                {
                    maximum = current;
                }
                ++lines;
                current = word;
                pending = 1;
            }
            else
            {
                current = combined;
                while (*text != 0 && *text != u' ')
                {
                    ++text;
                }
                --text;
                pending = 0;
            }
        }
        ++text;
    }
    if (pending != 0)
    {
        --lines;
    }
    if (current > maximum)
    {
        maximum = current;
    }
    const float height = float(state.fonts[font]->drop) * (scale != 0.f ? scale : state.scale_y);
    dimensions[0] = maximum;
    dimensions[1] = float(lines) * height;
    return lines;
}

// Xbox 0x00060270
const char16_t* parse_dialogue_text_transform(const char16_t* text, float& scale,
    float& x, float& y)
{
    scale = 1.f;
    x = 0.f;
    y = 0.f;
    if (*text == 0xAC)
    {
        scene_detail::crt_scan_wide_string(text + 1, u"%f %f %f", &x, &y, &scale);
        char16_t value = *text;
        while (value != 0)
        {
            if (value == 0xAC)
            {
                return text + 1;
            }
            ++text;
            value = *text;
        }
        if (*text == 0xAC)
        {
            return text + 1;
        }
    }
    return text;
}

// Xbox 0x00051760
i32 runtime_text_glyph_advance(const RuntimeTextFont& font, u16 code)
{
    if (code != 0)
    {
        if (font.first <= i32(code) && i32(code) <= font.last)
        {
            const auto& glyph = font.glyphs[code - font.first];
            if (glyph.texture != nullptr && u32(glyph.marker) != 0xFFFFFFFFu)
            {
                return std::bit_cast<i32>(u32(glyph.right) - u32(glyph.left));
            }
        }
    }
    return font.cell;
}

// Xbox 0x000607F0
float runtime_text_space_advance(const RuntimeTextFont& font, float scale, float default_scale)
{
    if (scale != 0.f)
    {
        if (font.first < 0x6D && 0x6B < font.last)
        {
            const auto& glyph = font.glyphs[0x6C - font.first];
            if (glyph.texture != nullptr && u32(glyph.marker) != 0xFFFFFFFFu)
            {
                return float(std::bit_cast<i32>(u32(glyph.right) - u32(glyph.left))) * scale;
            }
        }
        return float(font.cell) * scale;
    }
    if (font.first < 0x6D && 0x6B < font.last)
    {
        const auto& glyph = font.glyphs[0x6C - font.first];
        if (glyph.texture != nullptr && u32(glyph.marker) != 0xFFFFFFFFu)
        {
            return float(std::bit_cast<i32>(u32(glyph.right) - u32(glyph.left))) * default_scale;
        }
    }
    return float(font.cell) * default_scale;
}

// Xbox 0x0005F250
void apply_runtime_text_kerning(const RuntimeTextKerning& table, u16 first,
    u16 second, const float* scales, float* x, float* y)
{
    if (first != 0 && second != 0 && table.bound != 0)
    {
        i32 low = 0;
        i32 high = table.bound;
        i32 middle = table.bound;
        while (middle /= 2, low <= high)
        {
            const size_t offset = middle * 0xC;
            const u16 entry_first = read_u16(table.pairs, offset);
            bool less = first < entry_first;
            if (first == entry_first)
            {
                const u16 entry_second = read_u16(table.pairs, offset + 2);
                less = second < entry_second;
                if (second == entry_second)
                {
                    const float horizontal = read_f32(table.pairs, offset + 4);
                    if (x != nullptr && horizontal != 0.f)
                    {
                        *x = scales[0] * horizontal + *x;
                    }
                    if (y == nullptr)
                    {
                        return;
                    }
                    const float vertical = read_f32(table.pairs, offset + 8);
                    if (vertical == 0.f)
                    {
                        return;
                    }
                    *y = scales[1] * vertical + *y;
                    return;
                }
            }
            if (less)
            {
                high = middle - 1;
                middle = low + high;
            }
            else
            {
                low = middle + 1;
                middle = low + high;
            }
        }
    }
}

// Xbox 0x00060980
float runtime_text_character_advance(const RuntimeTextMetrics& state, i32 font,
    u16 previous, u16 current, float scale)
{
    float kerning = 0.f;
    float scales[2];
    if (scale != 0.f)
    {
        scales[0] = scale;
        scales[1] = scale;
    }
    else
    {
        scales[0] = state.scale_x;
        scales[1] = state.scale_y;
    }
    apply_runtime_text_kerning(state.kerning[font], previous, current, scales, &kerning, nullptr);
    float advance;
    if (current != 10)
    {
        if (current == 0x20)
        {
            advance = runtime_text_space_advance(*state.fonts[font], scale, state.scale_x);
        }
        else
        {
            advance = float(runtime_text_glyph_advance(*state.fonts[font], current));
        }
        if (scale != 0.f)
        {
            advance *= scale;
        }
        else
        {
            advance *= state.scale_x;
        }
    }
    else
    {
        advance = 0.f;
    }
    return (state.spacing + advance) + kerning;
}

// Xbox 0x00060710
float runtime_text_line_width(const RuntimeTextMetrics& state, i32 font,
    const char16_t* text, float scale)
{
    float width = 0.f;
    float maximum = 0.f;
    u16 previous = 0;
    if (*text != 0)
    {
        do
        {
            const u16 current = *text;
            if (current == 10)
            {
                if (width > maximum)
                {
                    maximum = width;
                }
                width = 0.f;
            }
            else if (current == 0x20)
            {
                width = runtime_text_space_advance(*state.fonts[font], scale, state.scale_x) + width;
            }
            else
            {
                width = runtime_text_character_advance(state, font, previous, current, scale) + width;
            }
            previous = *text;
            ++text;
        }
        while (*text != 0);
        if (width > maximum)
        {
            return width;
        }
    }
    return maximum;
}

// Xbox 0x00060790
float runtime_text_word_advance(const RuntimeTextMetrics& state, i32 font,
    const char16_t* text, u16 stop, float scale)
{
    float width = 0.f;
    u16 previous = 0;
    u16 current = *text;
    while (current != 0 && current != 10 && current != 0x20 && current != stop)
    {
        const float advance = runtime_text_character_advance(state, font, previous, current, scale);
        previous = *text;
        ++text;
        current = *text;
        width = advance + width;
    }
    return width;
}
}
