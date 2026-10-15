#include "actor_effect_draw.h"

#include <bit>
#include <cmath>

namespace gbtg
{
namespace
{
float real(std::span<const u8> bytes, size_t offset)
{
    return std::bit_cast<float>(read_u32(bytes, offset));
}

// 0x001673D8 (_ftol): truncation toward zero; the caller keeps the low byte.
u32 truncated_byte(float value)
{
    return u32(static_cast<i32>(std::trunc(value))) & 0xFF;
}
}

// Xbox 0x000EFEA0
void draw_actor_effect_particle(ActorEffectNode& effect, size_t particle, BillboardQueue& queue)
{
    const std::span<const u8> bytes = effect.bytes;
    const size_t at = 0x390 + particle * 0x150;
    const auto field = [&bytes, at](size_t offset)
    {
        return real(bytes, at + offset);
    };
    const u32 color = truncated_byte(field(0x60)) | truncated_byte(field(0x64)) << 8
        | truncated_byte(field(0x68)) << 16 | truncated_byte(field(0x6C)) << 24;
    // Particle +0x148 is its effect node.
    const float scale = real(bytes, 0xB8) * field(0x48);
    if (scale == 0.f) // DAT_0032B100
    {
        return;
    }
    std::array<float, 2> size{scale, scale};
    const u16 flags = read_u16(bytes, at + 0x58);
    if ((flags & 4) != 0)
    {
        size = {field(0xE8) * scale, field(0xEC) * scale};
    }
    Vec3 spin{field(0xC), field(0x10), field(0x14)};
    if ((flags & 8) != 0)
    {
        spin = {spin.x + field(0x118), spin.y + field(0x12C), spin.z + field(0x140)};
    }
    const Vec3 position{field(0), field(4), field(8)};
    const u32 frame = u32(static_cast<i32>(field(0x100)));
    switch (read_u32(bytes, 0x13C))
    {
    case 0:
        queue_billboard(queue, effect.texture.data(), color, frame, position, size.data(), spin.z);
        return;
    case 2:
        throw Error("untranslated Xbox 0x0005CA10: type-2 effect particle quad");
    }
}

// Xbox 0x000ED390
i32 draw_actor_effect_particles(ActorEffectNode& effect, BillboardQueue& queue)
{
    const u32 type = read_u32(effect.bytes, 0x13C);
    if (type == 2)
    {
        throw Error("untranslated Xbox 0x0005DFF0: type-2 effect draw states (after 0x0005CF20)");
    }
    i32 drawn = 0;
    const i32 count = static_cast<i16>(read_u16(effect.bytes, 2));
    u32 index = 0;
    if (0 < count)
    {
        do
        {
            if (static_cast<i16>(read_u16(effect.bytes, 0x390 + size_t(index & 0xFFFF) * 0x150 + 0x5A)) == -2)
            {
                switch (type)
                {
                case 0:
                    draw_actor_effect_particle(effect, size_t(index & 0xFFFF), queue);
                    break;
                case 4:
                    throw Error("untranslated Xbox 0x000F0160: type-4 effect particle model");
                }
                ++drawn;
            }
            ++index;
        }
        while (static_cast<i32>(index & 0xFFFF) < count);
    }
    return drawn;
}
}
