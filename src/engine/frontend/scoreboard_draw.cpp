#include "scoreboard_draw.h"
#include "gbtg/anim.h"

#include <bit>

namespace gbtg
{
// Xbox 0x001221B0
void queue_runtime_hud_sprite(const u8* texture, const float* position, const float* scale, float angle, u32 color,
    u8 frame, RuntimeScoreboardDraw& draw)
{
    auto& queue = draw.sprites;
    if (queue.count < 0 || size_t(queue.count) >= queue.records.size())
    {
        throw Error("HUD sprite queue DAT_00511210 is full");
    }
    auto& record = queue.records[size_t(queue.count)];
    record.texture = texture;
    record.position = {position[0], position[1]};
    record.scale = {scale[0], scale[1]};
    record.angle = angle;
    record.color = color;
    record.frame = frame;
    ++queue.count;
}

// Xbox 0x00103850
long double ease_runtime_hud(float time)
{
    const float angle = time * std::bit_cast<float>(0x40490FDBu) - std::bit_cast<float>(0x3FC90FDBu); // DAT_0032B148,
    return (retail_sin(angle) + 1.0L) * 0.5L; // DAT_0032B140, then DAT_0032B0F8 and DAT_0032B0F4
}

// Xbox 0x00131F00
bool place_runtime_hud_element(const RuntimeHudFade& fade, const float* hidden, const float* shown, float* output)
{
    switch (fade.state)
    {
    case 1:
    {
        const long double t = ease_runtime_hud(fade.second * (5.f / 3.f)); // DAT_0032B118
        output[0] = float((static_cast<long double>(hidden[0]) - shown[0]) * t + shown[0]);
        output[1] = float((static_cast<long double>(hidden[1]) - shown[1]) * t + shown[1]);
        return true;
    }
    case 2:
    case 3:
    case 5:
        output[0] = shown[0];
        output[1] = shown[1];
        return true;
    case 4:
    {
        const long double t = ease_runtime_hud(fade.second * 1.25f); // DAT_0032B114
        output[0] = float((static_cast<long double>(shown[0]) - hidden[0]) * t + hidden[0]);
        output[1] = float((static_cast<long double>(shown[1]) - hidden[1]) * t + hidden[1]);
        return true;
    }
    case 6:
        output[0] = hidden[0];
        output[1] = hidden[1];
        return false;
    default:
        return false;
    }
}

namespace
{
void draw_glyph(u32 now, const u8* texture, const float* scale, float* position, std::span<const u8> preset,
    RuntimeScoreboardDraw& draw)
{
    if (texture == nullptr)
    {
        throw Error("HUD glyph texture is not loaded");
    }
    if (now != 0)
    {
        throw Error("untranslated Xbox 0x0005CDC0 / 0x0005CA60 (a HUD glyph drawn at once)");
    }
    queue_runtime_hud_sprite(texture, position, scale, 0.f, read_u32(preset, 0x10), 0, draw); // 0x001221B0
    const i32 width = i32(u32(read_u16({texture, 0xA}, 8)) >> 1) + std::bit_cast<i32>(read_u32(preset, 0xC));
    position[0] = float(width) * read_f32(preset, 0) + position[0];
}
}

// Xbox 0x0012F8F0
void draw_runtime_hud_digit(u32 now, i32 digit, const float* scale, float* position, std::span<const u8> preset,
    RuntimeScoreboardHud& hud, RuntimeScoreboardDraw& draw)
{
    draw_glyph(now, hud.digits.at(size_t(digit)), scale, position, preset, draw); // DAT_00545A80
}

// Xbox 0x0012F850
void draw_runtime_hud_dollar(u32 now, i32 index, const float* scale, float* position, std::span<const u8> preset,
    RuntimeScoreboardHud& hud, RuntimeScoreboardDraw& draw)
{
    draw_glyph(now, hud.globals.dollars.at(size_t(index)), scale, position, preset, draw); // DAT_00545AA8
}

// Xbox 0x0012F000
void draw_runtime_hud_counter(RuntimeHudCounter& counter, float* position, std::span<const u8> preset, u32 now,
    RuntimeScoreboardHud& hud, RuntimeScoreboardDraw& draw)
{
    const float swell = std::bit_cast<float>(counter.word_2c); // +0x2C
    const float scale[2]{swell, swell};
    bool started = false;
    for (i32 index = 0; index < 8; ++index)
    {
        const i32 digit = counter.digits[size_t(index)];
        if (digit != 0 || index == 7 || started)
        {
            draw_runtime_hud_digit(now, digit, scale, position, preset, hud, draw); // 0x0012F8F0
            started = true;
        }
    }
}

// Xbox 0x0012F060
void draw_runtime_hud_two_digits(float* position, RuntimeHudCounter& counter, std::span<const u8> preset, u32 now,
    RuntimeScoreboardHud& hud, RuntimeScoreboardDraw& draw)
{
    const float swell = std::bit_cast<float>(counter.word_2c);
    const float scale[2]{swell, swell};
    const i32 value = std::bit_cast<i32>(counter.word_28);
    draw_runtime_hud_digit(now, value % 10, scale, position, preset, hud, draw);
    draw_runtime_hud_digit(now, value / 10, scale, position, preset, hud, draw);
}

// Xbox 0x0012F0D0
void draw_runtime_hud_money(float* position, RuntimeHudCounter& counter, std::span<const u8> preset, u32 now,
    RuntimeScoreboardHud& hud, RuntimeScoreboardDraw& draw)
{
    const float swell = std::bit_cast<float>(counter.word_2c);
    const float scale[2]{swell, swell};
    draw_runtime_hud_dollar(now, 0, scale, position, preset, hud, draw); // 0x0012F850, EAX the stack flag
    draw_runtime_hud_counter(counter, position, preset, 0, hud, draw); // 0x0012F000 with a pushed 0
}
}
