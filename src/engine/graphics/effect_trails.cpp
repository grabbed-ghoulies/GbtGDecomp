#include "effect_trails.h"
#include "effect_channels.h"
#include "extended_float.h"
#include "../entities/bounded_channels.h"
#include "../runtime/random_state.h"
#include "../audio/listener_settings.h"
#include "../entities/entity_heap.h"
#include "gbtg/anim.h"
#include <bit>
#include <cstring>

namespace gbtg
{
// Xbox 0x000EBB10
void bind_effect_trail_textures(RuntimeEffectTrailStorage& storage,
    AnimationResourceRegistry& registry, ResourceErrorState& error,
    const std::function<u32(const u8*)>& address)
{
    const std::span<u8> types{const_cast<u8*>(storage.types.data()), storage.types.size()};
    size_t index = 0;
    do
    {
        const auto type = record(types, effect_trail_types, index);
        std::span<const u8> texture;
        const char* name = field_data(type, effect_trail_type_field::texture);
        if (std::memcmp(name, "aid_", 4) == 0)
        {
            texture = acquire_animation_resource(registry, name);
            if (texture.data() == nullptr)
            {
                resource_error(error, 2);
            }
        }
        storage.textures[index] = texture;
        set(type, effect_trail_type_field::texture_address, address(texture.data()));
        ++index;
    }
    while (index < effect_trail_types.count);
}

// Xbox 0x000EBB80
void initialize_effect_trails(RuntimeEffectTrailStorage& storage,
    AnimationResourceRegistry& registry, ResourceErrorState& error,
    const std::function<u32(const u8*)>& address)
{
    storage.particles = static_cast<u8*>(engine_heap_allocate(effect_trail_pool.size));
    storage.free_head = 0;
    const std::span<u8> particles{storage.particles, effect_trail_pool.size};
    i32 index = 0;
    do
    {
        ++index;
        set(record(particles, effect_trail_pool, size_t(index - 1)), effect_trail_field::next_free, index);
    }
    while (index < 49);
    set(record(particles, effect_trail_pool, 49), effect_trail_field::next_free, -1);
    storage.types = acquire_animation_resource(registry, "aid_misc_ghoulies_fx_cheap_dust");
    if (storage.types.data() == nullptr)
    {
        resource_error(error, 2);
    }
    bind_effect_trail_textures(storage, registry, error, address);
}

// Xbox 0x000EBBF0
void release_effect_trails(RuntimeEffectTrailStorage& storage,
    AnimationResourceRegistry& registry)
{
    if (storage.particles != nullptr)
    {
        const std::span<u8> particles{storage.particles, effect_trail_pool.size};
        i32 index = 0;
        do
        {
            const auto particle = record(particles, effect_trail_pool, size_t(index));
            if (get(particle, effect_trail_field::next_free) == -2)
            {
                set(particle, effect_trail_field::next_free, storage.free_head);
                storage.free_head = index;
            }
            ++index;
        }
        while (index < 50);
        engine_heap_free(storage.particles);
        storage.particles = nullptr;
    }
    if (storage.types.data() != nullptr)
    {
        const std::span<u8> types{const_cast<u8*>(storage.types.data()), storage.types.size()};
        for (size_t index = 0; index < 4; ++index)
        {
            release_animation_resource(registry, storage.textures[index]);
            storage.textures[index] = {};
            set(record(types, effect_trail_types, index), effect_trail_type_field::texture_address, 0);
        }
        release_animation_resource(registry, storage.types);
        storage.types = {};
    }
}

// Xbox 0x000EB6C0
void set_effect_trail_phase(std::span<u8> particle, i32 phase,
    RuntimeEffectTrails& context)
{
    const auto type = record(context.types, effect_trail_types, get(particle, effect_trail_field::type));
    set(particle, effect_trail_field::phase, u8(phase));
    if (phase == 1)
    {
        set(particle, effect_trail_field::phase_time, advance_runtime_random_fraction(context.random) * .25f);
    }
    else if (phase == 2)
    {
        const float fraction = advance_runtime_random_fraction(context.random);
        const auto resource = context.resource(get(type, effect_trail_type_field::texture_address));
        const float extent = float(resource[0x11]) - 1.0f;
        const float complement = 1.0f - fraction;
        const auto channels = record(particle, effect_trail_field::channels);
        const auto first = get(type, effect_trail_type_field::first_colour);
        const auto second = get(type, effect_trail_type_field::second_colour);
        float* const values = field_data(channels, effect_channel_field::values);
        for (i32 index = 3; index >= 0; --index)
        {
            const long double value = static_cast<long double>(second[size_t(index)]) *
                complement + static_cast<long double>(first[size_t(index)]) * fraction;
            values[index] = float(u8(truncate_runtime_float(value)));
        }
        set(channels, effect_channel_field::unk_10, 255.0f);
        const float duration = get(particle, effect_trail_field::phase_time);
        set(channels, effect_channel_field::target_alpha, 128.0f); // DAT_0032B368 = 0x43000000.
        if (duration > std::bit_cast<float>(0x3456BF95u))
        {
            const float difference = float(ExtendedFloat{128.0f} -
                ExtendedFloat{get(channels, effect_channel_field::alpha)});
            set(channels, effect_channel_field::alpha_rate,
                float(ExtendedFloat{engine_absolute(difference)} / ExtendedFloat{duration}));
        }
        else
        {
            set(channels, effect_channel_field::alpha, 128.0f);
            set(channels, effect_channel_field::alpha_rate, 0.0f);
        }
        const float frame_rate = get(type, effect_trail_type_field::frame_rate);
        if (extent > 0.0f)
        {
            set(particle, effect_trail_field::phase_time, extent / frame_rate);
        }
        else
        {
            set(particle, effect_trail_field::phase_time, get(particle, effect_trail_field::life));
        }
        const float scale = get(particle, effect_trail_field::scale);
        set(particle, effect_trail_field::size, get(type, effect_trail_type_field::start_size) * scale);
        set(particle, effect_trail_field::target_size, get(type, effect_trail_type_field::end_size) * scale);
        const float rate = (get(particle, effect_trail_field::target_size) - get(particle, effect_trail_field::size)) /
            get(particle, effect_trail_field::phase_time);
        set(particle, effect_trail_field::size_rate, float(engine_absolute(rate)));
        const auto frames = record(particle, effect_trail_field::frames);
        set(frames, bounded_channel_field::lower, 0.0f);
        set(frames, bounded_channel_field::upper, 0.0f);
        set(frames, bounded_channel_field::rate, 0.0f);
        set(frames, bounded_channel_field::value, 0.0f);
        set(frames, bounded_channel_field::mode, 0);
        set_runtime_bounded_channel_limits(frames, 0.0f, extent, context.pal, context.override_pal);
        set(frames, bounded_channel_field::rate, frame_rate);
        const i32 frequency = context.pal != 0 && context.override_pal == 0 ? 50 : 60;
        const float maximum = (float(frequency) * .5f) *
            (get(frames, bounded_channel_field::upper) - get(frames, bounded_channel_field::lower));
        if (frame_rate > maximum)
        {
            set(frames, bounded_channel_field::rate, maximum);
        }
        if (get(frames, bounded_channel_field::lower) != get(frames, bounded_channel_field::upper))
        {
            set(frames, bounded_channel_field::mode, 1);
        }
    }
}

// Xbox 0x000EB9F0
void tick_effect_trail(std::span<u8> particle, RuntimeEffectTrails& context,
    const RuntimeActorClock& clock)
{
    const float step = runtime_frame_step(clock);
    approach_actor_effect_channels(record(particle, effect_trail_field::channels), clock);
    set(particle, effect_trail_field::life, get(particle, effect_trail_field::life) - step);
    const u8 phase = get(particle, effect_trail_field::phase);
    if (phase == 1)
    {
        if (get(particle, effect_trail_field::phase_time) > 0.0f)
        {
            set(particle, effect_trail_field::phase_time, get(particle, effect_trail_field::phase_time) - step);
            return;
        }
    }
    else if (phase == 2)
    {
        if (get(particle, effect_trail_field::phase_time) > 0.0f)
        {
            set(particle, effect_trail_field::phase_time, get(particle, effect_trail_field::phase_time) - step);
            float current = get(particle, effect_trail_field::size);
            float rate = get(particle, effect_trail_field::size_rate);
            approach_runtime_scalar_rate(current, rate, get(particle, effect_trail_field::target_size), step);
            set(particle, effect_trail_field::size, current);
            set(particle, effect_trail_field::size_rate, rate);
            const auto frames = record(particle, effect_trail_field::frames);
            if ((get(frames, bounded_channel_field::mode) & 2) != 0)
            {
                decrease_runtime_bounded_channel(frames, step);
            }
            else if ((get(frames, bounded_channel_field::mode) & 1) != 0)
            {
                increase_runtime_bounded_channel(frames, step);
            }
            return;
        }
    }
    else
    {
        return;
    }
    if (get(particle, effect_trail_field::life) > 0.0f)
    {
        set_effect_trail_phase(particle, 2, context);
    }
    else
    {
        const i32 index = i32((particle.data() - context.particles.data()) / effect_trail_field::kSize);
        set(record(context.particles, effect_trail_pool, size_t(index)), effect_trail_field::next_free,
            context.free_head);
        context.free_head = index;
    }
}
}
