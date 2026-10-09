#pragma once

#include "dialogue_text_pool.h"

namespace gbtg
{
struct RuntimeTextKerning
{
    i32 bound; // DAT_005043BC
    std::span<const u8> pairs; // DAT_005043C0, stride 0xC
};
struct RuntimeTextMetrics
{
    std::span<const RuntimeTextFont* const> fonts;
    std::span<const RuntimeTextKerning> kerning;
    float scale_x; // DAT_005043E4
    float scale_y; // DAT_005043E8
    float spacing; // DAT_005043F4
};
const char16_t* parse_dialogue_text_transform(const char16_t* text, float& scale,
    float& x, float& y); // Xbox 0x00060270
i32 runtime_text_glyph_advance(const RuntimeTextFont& font, u16 code); // Xbox 0x00051760
float runtime_text_space_advance(const RuntimeTextFont& font, float scale,
    float default_scale); // Xbox 0x000607F0
void apply_runtime_text_kerning(const RuntimeTextKerning& table, u16 first,
    u16 second, const float* scales, float* x, float* y); // Xbox 0x0005F250
float runtime_text_character_advance(const RuntimeTextMetrics& state, i32 font,
    u16 previous, u16 current, float scale); // Xbox 0x00060980
float runtime_text_line_width(const RuntimeTextMetrics& state, i32 font,
    const char16_t* text, float scale); // Xbox 0x00060710
float runtime_text_word_advance(const RuntimeTextMetrics& state, i32 font,
    const char16_t* text, u16 stop, float scale); // Xbox 0x00060790
i32 measure_runtime_text_block(const RuntimeTextMetrics& state, i32 font,
    const char16_t* text, float scale, float wrap_width, float viewport_width,
    float* dimensions); // Xbox 0x00060A40
}
