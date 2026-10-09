#pragma once

#include "gbtg/types.h"

namespace gbtg
{
struct GhoulySpawnPool;
struct RuntimeTextMetrics;
struct RuntimeTextGlyph
{
    const u8* texture; // font glyph +0, actual relocated texture
    u16 marker; // +4
    i32 left = 0; // +8
    i32 right = 0; // +0x10
};
struct RuntimeTextFont
{
    i32 first;
    i32 last;
    std::span<const RuntimeTextGlyph> glyphs; // +0x14, retail stride 0x18
    i32 cell = 0; // +0xC
    i32 drop = 0; // +0x10
};
struct RuntimeDialogueTextPool
{
    std::span<u8> records; // active pool +4..+8, stride 0x1238
    i32 font; // +4
    float end_time; // +0x20
    GhoulySpawnPool* allocation = nullptr; // +0, native heap pool header
    float wrap_width = 0.f; // +0x10
    u32 variant = 0; // +8
    u32 flags = 0; // +0xC
    std::array<float, 3> origin{}; // +0x14..+0x1C
    u32 enabled = 0; // +0x24
    std::array<float, 2> offsets{}; // +0x28..+0x2C
};
void allocate_dialogue_text_pool(RuntimeDialogueTextPool*& output, i32 font,
    u32 variant, u32 flags, GhoulySpawnPool*& registry); // Xbox 0x0005F390
void release_dialogue_text_pool(RuntimeDialogueTextPool* pool,
    GhoulySpawnPool& registry); // Xbox 0x0005F460
u32 end_dialogue_text_record(RuntimeDialogueTextPool& pool, u32 id,
    std::span<const RuntimeTextFont* const> fonts, const u8* fallback_texture,
    u32& random_state); // Xbox 0x0005F7E0
u32 create_dialogue_text_record(RuntimeDialogueTextPool& pool, const char16_t* text,
    float x, float y, float scale_x, float scale_y, float lifetime,
    i32 centered, i32 panel, i32 sticky, const RuntimeTextMetrics& metrics,
    float viewport_width, i32& next_id, u32& random_state); // Xbox 0x0005F5C0
}
