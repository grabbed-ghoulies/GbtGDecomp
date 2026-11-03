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

// Xbox 0x000EF1D0
u32 actor_effect_emission_count(ActorEffectNode& effect, float step)
{
    const float interval = get(effect.bytes, actor_effect_field::emitting);
    u32 count = 0;
    if (interval != -1.f && interval != -3.f)
    {
        if (interval == -2.f)
        {
            return u32(i32(std::bit_cast<i16>(get(effect.bytes, actor_effect_field::capacity))) -
                i32(std::bit_cast<i16>(get(effect.bytes, actor_effect_field::used_count))));
        }
        if (0.f >= get(effect.bytes, actor_effect_field::emit_timer))
        {
            float timer = get(effect.bytes, actor_effect_field::emit_timer);
            do
            {
                timer += get(effect.bytes, actor_effect_field::emitting);
                ++count;
            }
            while (0.f >= timer);
            set(effect.bytes, actor_effect_field::emit_timer, timer);
        }
        if (get(effect.bytes, actor_effect_field::emit_timer) > 0.f - step)
        {
            set(effect.bytes, actor_effect_field::emit_timer, get(effect.bytes, actor_effect_field::emit_timer) - step);
        }
    }
    return count;
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
    set(particle, effect_particle_field::unk_108, 0.f);
    set(particle, effect_particle_field::target_size, 1.f);
    for (size_t offset = 0xA8; offset <= 0xB4; offset += 4)
    {
        number(particle, offset, -1.f);
    }
    set(particle, effect_particle_field::collisions, 0);
    const u16 zero = 0;
    std::memcpy(particle.data() + 0x58, &zero, 2);
    set(particle, effect_particle_field::frame_mode, 0);
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
    set(particle, effect_particle_field::bounce, 1.f);
    set(particle, effect_particle_field::spin_bounce, 1.f);
    set(particle, effect_particle_field::size, 1.f);
    set(particle, effect_particle_field::unk_55, 1);
    const float variance = get(effect.bytes, actor_effect_field::size_variance);
    const float minimum = 1.f - variance;
    const float range = (variance + 1.f) - minimum;
    const float fraction = random_fraction(random);
    const __m128 varied = _mm_add_ss(_mm_mul_ss(_mm_set_ss(range), _mm_set_ss(fraction)), _mm_set_ss(minimum));
    const float scale =
        _mm_cvtss_f32(_mm_mul_ss(varied, _mm_set_ss(get(effect.bytes, actor_effect_field::particle_size))));
    set(particle, effect_particle_field::size_scale, scale);
}

// Xbox 0x000EFB20
void initialize_actor_effect_particle_frames(std::span<u8> particle, const ActorEffectNode& effect,
    u32& random, i32 pal, i32 pal_override)
{
    const float first = get(effect.bytes, actor_effect_field::frame_rate_low);
    const float second = get(effect.bytes, actor_effect_field::frame_rate_high);
    const bool varies = first != 0.f || second != 0.f;
    const u8 flags = get(effect.bytes, actor_effect_field::frame_flags);
    if (!varies && (flags & 1) == 0)
    {
        return;
    }
    const u32 count = get(effect.bytes, actor_effect_field::frame_count);
    if (count < 2)
    {
        return;
    }
    u8 mode = 0;
    const float frames = float(count);
    if (0.f > frames)
    {
        set(particle, effect_particle_field::first_frame, frames);
        set(particle, effect_particle_field::last_frame, 0.f);
    }
    else if (!(frames > 0.f))
    {
        particle[0x104] &= 0xFC;
        set(particle, effect_particle_field::first_frame, frames);
        set(particle, effect_particle_field::last_frame, 0.f);
    }
    else
    {
        set(particle, effect_particle_field::first_frame, 0.f);
        set(particle, effect_particle_field::last_frame, frames);
    }
    if (get(particle, effect_particle_field::frame) > get(particle, effect_particle_field::last_frame))
    {
        set(particle, effect_particle_field::frame, get(particle, effect_particle_field::last_frame));
    }
    else if (get(particle, effect_particle_field::first_frame) > get(particle, effect_particle_field::frame))
    {
        set(particle, effect_particle_field::frame, get(particle, effect_particle_field::first_frame));
    }
    const i32 rate = pal != 0 && pal_override == 0 ? 50 : 60;
    const float maximum = (float(rate) * .5f) * (get(particle, effect_particle_field::last_frame) -
        get(particle, effect_particle_field::first_frame));
    if (get(particle, effect_particle_field::frame_speed) > maximum)
    {
        set(particle, effect_particle_field::frame_speed, maximum);
    }
    const float fraction = random_fraction(random);
    const float speed = (second - first) * fraction + first;
    set(particle, effect_particle_field::frame_speed, speed);
    if (speed > maximum)
    {
        set(particle, effect_particle_field::frame_speed, maximum);
    }
    if ((flags & 1) != 0)
    {
        const float fraction = random_fraction(random);
        const float position = fraction * float(count + 1u);
        set(particle, effect_particle_field::frame, position);
        if (position > get(particle, effect_particle_field::last_frame))
        {
            set(particle, effect_particle_field::frame, get(particle, effect_particle_field::last_frame));
        }
        else if (get(particle, effect_particle_field::first_frame) > position)
        {
            set(particle, effect_particle_field::frame, get(particle, effect_particle_field::first_frame));
        }
    }
    else
    {
        set(particle, effect_particle_field::frame, 0.f);
        if (0.f > get(particle, effect_particle_field::last_frame))
        {
            set(particle, effect_particle_field::frame, get(particle, effect_particle_field::last_frame));
        }
        else if (get(particle, effect_particle_field::first_frame) > 0.f)
        {
            set(particle, effect_particle_field::frame, get(particle, effect_particle_field::first_frame));
        }
    }
    if ((get(effect.bytes, actor_effect_field::frame_flags) & 8) != 0)
    {
        mode = random_fraction(random) > .5f ? 5 : 6;
    }
    else if ((get(effect.bytes, actor_effect_field::frame_flags) & 4) != 0)
    {
        mode = 9;
    }
    else if ((get(effect.bytes, actor_effect_field::frame_flags) & 0x10) == 0)
    {
        mode = 1;
    }
    if (get(particle, effect_particle_field::first_frame) != get(particle, effect_particle_field::last_frame))
    {
        set(particle, effect_particle_field::frame_mode, mode);
    }
}

// Xbox 0x000EFE30
u8* allocate_actor_effect_particle(ActorEffectNode& effect, u32 owner_address, u32 user,
    u32& random, i32 pal, i32 pal_override)
{
    const i16 slot = std::bit_cast<i16>(get(effect.bytes, actor_effect_field::free_head));
    if (slot != -1)
    {
        const size_t offset = size_t(slot) * 0x150 + 0x390;
        auto particle = effect.bytes.subspan(offset, 0x150);
        const u16 next = get(particle, effect_particle_field::next_free);
        set(effect.bytes, actor_effect_field::free_head, next);
        set(particle, effect_particle_field::command, user);
        const u16 active = 0xFFFE;
        std::memcpy(particle.data() + 0x5A, &active, 2);
        set(particle, effect_particle_field::owner, owner_address);
        const u16 count = u16(get(effect.bytes, actor_effect_field::used_count) + 1);
        std::memcpy(effect.bytes.data(), &count, 2);
        initialize_actor_effect_particle(particle, effect, random);
        const u32 kind = get(effect.bytes, actor_effect_field::draw_mode);
        if (kind == 0 || kind == 2)
        {
            initialize_actor_effect_particle_frames(particle, effect, random, pal, pal_override);
        }
        return particle.data();
    }
    return nullptr;
}
}
