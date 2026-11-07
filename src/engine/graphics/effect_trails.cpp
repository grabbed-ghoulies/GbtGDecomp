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
    storage.particles =
        static_cast<EffectTrailRecord*>(engine_heap_allocate(sizeof(EffectTrailRecord) * kEffectTrailCount));
    storage.free_head = 0;
    const std::span<EffectTrailRecord> particles{storage.particles, kEffectTrailCount};
    i32 index = 0;
    do
    {
        ++index;
        particles[size_t(index - 1)].next_free = index;
    }
    while (index < 49);
    particles[49].next_free = -1;
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
        const std::span<EffectTrailRecord> particles{storage.particles, kEffectTrailCount};
        i32 index = 0;
        do
        {
            EffectTrailRecord& particle = particles[size_t(index)];
            if (particle.next_free == -2)
            {
                particle.next_free = storage.free_head;
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
void set_effect_trail_phase(EffectTrailRecord& particle, i32 phase,
    RuntimeEffectTrails& context)
{
    const auto type = record(context.types, effect_trail_types, particle.type);
    particle.phase = u8(phase);
    if (phase == 1)
    {
        particle.phase_time = advance_runtime_random_fraction(context.random) * .25f;
    }
    else if (phase == 2)
    {
        const float fraction = advance_runtime_random_fraction(context.random);
        const auto resource = context.resource(get(type, effect_trail_type_field::texture_address));
        const float extent = float(resource[0x11]) - 1.0f;
        const float complement = 1.0f - fraction;
        auto& channels = particle.channels;
        const auto first = get(type, effect_trail_type_field::first_colour);
        const auto second = get(type, effect_trail_type_field::second_colour);
        float* const values = channels.values.data();
        for (i32 index = 3; index >= 0; --index)
        {
            const long double value = static_cast<long double>(second[size_t(index)]) *
                complement + static_cast<long double>(first[size_t(index)]) * fraction;
            values[index] = float(u8(truncate_runtime_float(value)));
        }
        channels.unk_10() = 255.0f;
        const float duration = particle.phase_time;
        channels.target_alpha() = 128.0f; // DAT_0032B368 = 0x43000000.
        if (duration > std::bit_cast<float>(0x3456BF95u))
        {
            const float difference = float(ExtendedFloat{128.0f} -
                ExtendedFloat{channels.alpha()});
            channels.alpha_rate() = float(ExtendedFloat{engine_absolute(difference)} / ExtendedFloat{duration});
        }
        else
        {
            channels.alpha() = 128.0f;
            channels.alpha_rate() = 0.0f;
        }
        const float frame_rate = get(type, effect_trail_type_field::frame_rate);
        if (extent > 0.0f)
        {
            particle.phase_time = extent / frame_rate;
        }
        else
        {
            particle.phase_time = particle.life;
        }
        const float scale = particle.scale;
        particle.size = get(type, effect_trail_type_field::start_size) * scale;
        particle.target_size = get(type, effect_trail_type_field::end_size) * scale;
        const float rate = (particle.target_size - particle.size) / particle.phase_time;
        particle.size_rate = float(engine_absolute(rate));
        BoundedChannel& frames = particle.frames;
        frames.lower = 0.0f;
        frames.upper = 0.0f;
        frames.rate = 0.0f;
        frames.value = 0.0f;
        frames.mode = 0;
        set_runtime_bounded_channel_limits(frames, 0.0f, extent, context.pal, context.override_pal);
        frames.rate = frame_rate;
        const i32 frequency = context.pal != 0 && context.override_pal == 0 ? 50 : 60;
        const float maximum = (float(frequency) * .5f) * (frames.upper - frames.lower);
        if (frame_rate > maximum)
        {
            frames.rate = maximum;
        }
        if (frames.lower != frames.upper)
        {
            frames.mode = 1;
        }
    }
}

// Xbox 0x000EB9F0
void tick_effect_trail(EffectTrailRecord& particle, RuntimeEffectTrails& context,
    const RuntimeActorClock& clock)
{
    const float step = runtime_frame_step(clock);
    approach_actor_effect_channels(particle.channels, clock);
    particle.life = particle.life - step;
    const u8 phase = particle.phase;
    if (phase == 1)
    {
        if (particle.phase_time > 0.0f)
        {
            particle.phase_time = particle.phase_time - step;
            return;
        }
    }
    else if (phase == 2)
    {
        if (particle.phase_time > 0.0f)
        {
            particle.phase_time = particle.phase_time - step;
            float current = particle.size;
            float rate = particle.size_rate;
            approach_runtime_scalar_rate(current, rate, particle.target_size, step);
            particle.size = current;
            particle.size_rate = rate;
            BoundedChannel& frames = particle.frames;
            if ((frames.mode & 2) != 0)
            {
                decrease_runtime_bounded_channel(frames, step);
            }
            else if ((frames.mode & 1) != 0)
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
    if (particle.life > 0.0f)
    {
        set_effect_trail_phase(particle, 2, context);
    }
    else
    {
        const i32 index = i32(&particle - context.particles.data());
        context.particles[size_t(index)].next_free = context.free_head;
        context.free_head = index;
    }
}
}
