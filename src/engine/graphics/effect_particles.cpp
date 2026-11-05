#include "effect_particles.h"
#include "effect_channels.h"

#include <bit>
#include <cstddef>
#include <cstring>
#include <xmmintrin.h>

namespace gbtg
{
namespace
{
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
void initialize_actor_effect_particle(EffectParticle& particle, const ActorEffectNode& effect, u32& random)
{
    auto& channels = particle.colour_channels;
    channels.unk_10() = 0.f;
    channels.red() = 255.f;
    channels.green() = 255.f;
    channels.blue() = 255.f;
    channels.alpha() = 255.f;
    channels.target_red() = 255.f;
    channels.target_green() = 255.f;
    channels.target_blue() = 255.f;
    channels.target_alpha() = 255.f;
    channels.target_unk_10() = 0.f;
    channels.rates = std::array<float, 5>{};
    particle.size_rate = 0.f;
    particle.unk_108 = 0.f;
    particle.target_size = 1.f;
    particle.life = -1.f;
    particle.duration_low = -1.f;
    particle.duration_high = -1.f;
    particle.duration = -1.f;
    particle.collisions = 0;
    particle.channel_flags = 0;
    particle.frames.mode = 0;
    particle.frames.lower = 0.f;
    particle.frames.upper = 0.f;
    particle.frames.rate = 0.f;
    particle.frames.value = 0.f;
    particle.oscillation = {};
    for (BoundedChannel& channel : particle.bounded_channels)
    {
        channel.mode = 0;
        channel.lower = 0.f;
        channel.upper = 0.f;
        channel.rate = 0.f;
        channel.value = 0.f;
    }
    // Everything before the channel flags: the position, the motion and the motion flags.
    std::memset(&particle, 0, offsetof(EffectParticle, channel_flags));
    particle.bounce = 1.f;
    particle.spin_bounce = 1.f;
    particle.size = 1.f;
    particle.unk_55 = 1;
    const float variance = get(effect.bytes, actor_effect_field::size_variance);
    const float minimum = 1.f - variance;
    const float range = (variance + 1.f) - minimum;
    const float fraction = random_fraction(random);
    const __m128 varied = _mm_add_ss(_mm_mul_ss(_mm_set_ss(range), _mm_set_ss(fraction)), _mm_set_ss(minimum));
    const float scale =
        _mm_cvtss_f32(_mm_mul_ss(varied, _mm_set_ss(get(effect.bytes, actor_effect_field::particle_size))));
    particle.size_scale = scale;
}

// Xbox 0x000EFB20
void initialize_actor_effect_particle_frames(EffectParticle& particle, const ActorEffectNode& effect,
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
        particle.frames.lower = frames;
        particle.frames.upper = 0.f;
    }
    else if (!(frames > 0.f))
    {
        particle.frames.mode &= 0xFC;
        particle.frames.lower = frames;
        particle.frames.upper = 0.f;
    }
    else
    {
        particle.frames.lower = 0.f;
        particle.frames.upper = frames;
    }
    if (particle.frames.value > particle.frames.upper)
    {
        particle.frames.value = particle.frames.upper;
    }
    else if (particle.frames.lower > particle.frames.value)
    {
        particle.frames.value = particle.frames.lower;
    }
    const i32 rate = pal != 0 && pal_override == 0 ? 50 : 60;
    const float maximum = (float(rate) * .5f) * (particle.frames.upper - particle.frames.lower);
    if (particle.frames.rate > maximum)
    {
        particle.frames.rate = maximum;
    }
    const float fraction = random_fraction(random);
    const float speed = (second - first) * fraction + first;
    particle.frames.rate = speed;
    if (speed > maximum)
    {
        particle.frames.rate = maximum;
    }
    if ((flags & 1) != 0)
    {
        const float fraction = random_fraction(random);
        const float position = fraction * float(count + 1u);
        particle.frames.value = position;
        if (position > particle.frames.upper)
        {
            particle.frames.value = particle.frames.upper;
        }
        else if (particle.frames.lower > position)
        {
            particle.frames.value = particle.frames.lower;
        }
    }
    else
    {
        particle.frames.value = 0.f;
        if (0.f > particle.frames.upper)
        {
            particle.frames.value = particle.frames.upper;
        }
        else if (particle.frames.lower > 0.f)
        {
            particle.frames.value = particle.frames.lower;
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
    if (particle.frames.lower != particle.frames.upper)
    {
        particle.frames.mode = mode;
    }
}

EffectParticle& actor_effect_particle(ActorEffectNode& effect, size_t index)
{
    const size_t offset = actor_effect_field::particles_offset + index * sizeof(EffectParticle);
    detail::check_retail_range(effect.bytes.size(), u32(offset), sizeof(EffectParticle));
    return *reinterpret_cast<EffectParticle*>(effect.bytes.data() + offset);
}

// Xbox 0x000EFE30
EffectParticle* allocate_actor_effect_particle(ActorEffectNode& effect, u32 owner_address, u32 user,
    u32& random, i32 pal, i32 pal_override)
{
    const i16 slot = std::bit_cast<i16>(get(effect.bytes, actor_effect_field::free_head));
    if (slot != -1)
    {
        EffectParticle& particle = actor_effect_particle(effect, size_t(slot));
        set(effect.bytes, actor_effect_field::free_head, particle.next_free);
        particle.command = user;
        particle.next_free = 0xFFFE;
        particle.owner = owner_address;
        set(effect.bytes, actor_effect_field::used_count, u16(get(effect.bytes, actor_effect_field::used_count) + 1));
        initialize_actor_effect_particle(particle, effect, random);
        const u32 kind = get(effect.bytes, actor_effect_field::draw_mode);
        if (kind == 0 || kind == 2)
        {
            initialize_actor_effect_particle_frames(particle, effect, random, pal, pal_override);
        }
        return &particle;
    }
    return nullptr;
}
}
