#include "effect_particles.h"

#include <bit>
#include <cstring>
#include <xmmintrin.h>

namespace gbtg
{
namespace
{
void word(std::span<u8> row, size_t offset, u32 value)
{
    std::memcpy(row.data() + offset, &value, 4);
}
void number(std::span<u8> row, size_t offset, float value)
{
    word(row, offset, std::bit_cast<u32>(value));
}
float random_fraction(u32& state)
{
    state = state * 0x19660Du + 0x3C6EF35Fu;
    return std::bit_cast<float>((state & 0x7FFFFFu) | 0x3F800000u) - 1.f;
}
}

// Xbox 0x000EF8C0
void initialize_actor_effect_particle(std::span<u8> particle, const ActorEffectNode& effect, u32& random)
{
    word(particle, 0x70, 0);
    for (size_t offset : {0x60u,0x64u,0x68u,0x6Cu,0x74u,0x78u,0x7Cu,0x80u})
    {
        number(particle, offset, 255.f);
    }
    for (size_t offset = 0x84; offset <= 0x9C; offset += 4)
    {
        word(particle, offset, 0);
    }
    word(particle, 0x108, 0);
    number(particle, 0xA0, 1.f);
    for (size_t offset = 0xA8; offset <= 0xB4; offset += 4)
    {
        number(particle, offset, -1.f);
    }
    word(particle, 0x5C, 0);
    const u16 zero = 0;
    std::memcpy(particle.data() + 0x58, &zero, 2);
    particle[0x104] = 0;
    for (size_t offset = 0xF4; offset <= 0x100; offset += 4)
    {
        word(particle, offset, 0);
    }
    for (size_t offset = 0xB8; offset <= 0xF0; offset += 4)
    {
        word(particle, offset, 0);
    }
    for (size_t base : {0x10Cu,0x120u,0x134u})
    {
        particle[base + 0x10] = 0;
        for (size_t offset = base; offset < base + 0x10; offset += 4)
        {
            word(particle, offset, 0);
        }
    }
    std::memset(particle.data(), 0, 0x58);
    number(particle, 0x40, 1.f);
    number(particle, 0x44, 1.f);
    number(particle, 0x48, 1.f);
    particle[0x55] = 1;
    const float variance = read_f32(effect.bytes, 0x9C);
    const float minimum = 1.f - variance;
    const float range = (variance + 1.f) - minimum;
    const float fraction = random_fraction(random);
    const __m128 varied = _mm_add_ss(_mm_mul_ss(_mm_set_ss(range), _mm_set_ss(fraction)), _mm_set_ss(minimum));
    const float scale = _mm_cvtss_f32(_mm_mul_ss(varied, _mm_set_ss(read_f32(effect.bytes, 0x98))));
    number(particle, 0xA4, scale);
}

// Xbox 0x000EFB20
void initialize_actor_effect_particle_frames(std::span<u8> particle, const ActorEffectNode& effect,
    u32& random, i32 pal, i32 pal_override)
{
    const float first = read_f32(effect.bytes, 0x14C);
    const float second = read_f32(effect.bytes, 0x150);
    const bool varies = first != 0.f || second != 0.f;
    const u8 flags = effect.bytes[0x154];
    if (!varies && (flags & 1) == 0)
    {
        return;
    }
    const u32 count = read_u32(effect.bytes, 0x148);
    if (count < 2)
    {
        return;
    }
    u8 mode = 0;
    const float frames = float(count);
    if (0.f > frames)
    {
        number(particle, 0xF4, frames);
        word(particle, 0xF8, 0);
    }
    else if (!(frames > 0.f))
    {
        particle[0x104] &= 0xFC;
        number(particle, 0xF4, frames);
        word(particle, 0xF8, 0);
    }
    else
    {
        word(particle, 0xF4, 0);
        number(particle, 0xF8, frames);
    }
    if (read_f32(particle, 0x100) > read_f32(particle, 0xF8))
    {
        word(particle, 0x100, read_u32(particle, 0xF8));
    }
    else if (read_f32(particle, 0xF4) > read_f32(particle, 0x100))
    {
        word(particle, 0x100, read_u32(particle, 0xF4));
    }
    const i32 rate = pal != 0 && pal_override == 0 ? 50 : 60;
    const float maximum = (float(rate) * .5f) * (read_f32(particle, 0xF8) - read_f32(particle, 0xF4));
    if (read_f32(particle, 0xFC) > maximum)
    {
        number(particle, 0xFC, maximum);
    }
    const float fraction = random_fraction(random);
    const float speed = (second - first) * fraction + first;
    number(particle, 0xFC, speed);
    if (speed > maximum)
    {
        number(particle, 0xFC, maximum);
    }
    if ((flags & 1) != 0)
    {
        const float fraction = random_fraction(random);
        const float position = fraction * float(count + 1u);
        number(particle, 0x100, position);
        if (position > read_f32(particle, 0xF8))
        {
            word(particle, 0x100, read_u32(particle, 0xF8));
        }
        else if (read_f32(particle, 0xF4) > position)
        {
            word(particle, 0x100, read_u32(particle, 0xF4));
        }
    }
    else
    {
        word(particle, 0x100, 0);
        if (0.f > read_f32(particle, 0xF8))
        {
            word(particle, 0x100, read_u32(particle, 0xF8));
        }
        else if (read_f32(particle, 0xF4) > 0.f)
        {
            word(particle, 0x100, read_u32(particle, 0xF4));
        }
    }
    if ((effect.bytes[0x154] & 8) != 0)
    {
        mode = random_fraction(random) > .5f ? 5 : 6;
    }
    else if ((effect.bytes[0x154] & 4) != 0)
    {
        mode = 9;
    }
    else if ((effect.bytes[0x154] & 0x10) == 0)
    {
        mode = 1;
    }
    if (read_f32(particle, 0xF4) != read_f32(particle, 0xF8))
    {
        particle[0x104] = mode;
    }
}

// Xbox 0x000EFE30
u8* allocate_actor_effect_particle(ActorEffectNode& effect, u32 owner_address, u32 user,
    u32& random, i32 pal, i32 pal_override)
{
    const i16 slot = std::bit_cast<i16>(read_u16(effect.bytes, 6));
    if (slot != -1)
    {
        const size_t offset = size_t(slot) * 0x150 + 0x390;
        auto particle = effect.bytes.subspan(offset, 0x150);
        const u16 next = read_u16(particle, 0x5A);
        std::memcpy(effect.bytes.data() + 6, &next, 2);
        word(particle, 0x14C, user);
        const u16 active = 0xFFFE;
        std::memcpy(particle.data() + 0x5A, &active, 2);
        word(particle, 0x148, owner_address);
        const u16 count = u16(read_u16(effect.bytes, 0) + 1);
        std::memcpy(effect.bytes.data(), &count, 2);
        initialize_actor_effect_particle(particle, effect, random);
        const u32 kind = read_u32(effect.bytes, 0x13C);
        if (kind == 0 || kind == 2)
        {
            initialize_actor_effect_particle_frames(particle, effect, random, pal, pal_override);
        }
        return particle.data();
    }
    return nullptr;
}
}
