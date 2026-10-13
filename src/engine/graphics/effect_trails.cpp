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
namespace
{
void store_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}
}

// Xbox 0x000EBB10
void bind_effect_trail_textures(RuntimeEffectTrailStorage& storage,
    AnimationResourceRegistry& registry, ResourceErrorState& error,
    const std::function<u32(const u8*)>& address)
{
    const std::span<u8> types{const_cast<u8*>(storage.types.data()), storage.types.size()};
    size_t offset = 0;
    do
    {
        std::span<const u8> texture;
        if (read_u32(types, offset + 4) == 0x5F646961u)
        {
            texture = acquire_animation_resource(registry,
                reinterpret_cast<const char*>(types.data() + offset + 4));
            if (texture.data() == nullptr)
            {
                resource_error(error, 2);
            }
        }
        storage.textures[offset / 0xA0] = texture;
        const u32 pointer = address(texture.data());
        std::memcpy(types.data() + offset, &pointer, 4);
        offset += 0xA0;
    }
    while (offset < 0x280);
}

// Xbox 0x000EBB80
void initialize_effect_trails(RuntimeEffectTrailStorage& storage,
    AnimationResourceRegistry& registry, ResourceErrorState& error,
    const std::function<u32(const u8*)>& address)
{
    storage.particles = static_cast<u8*>(engine_heap_allocate(0x19C8));
    storage.free_head = 0;
    i32 index = 0;
    do
    {
        ++index;
        std::memcpy(storage.particles + size_t(index - 1) * 0x84 + 0x7C, &index, 4);
    }
    while (index < 49);
    const i32 end = -1;
    std::memcpy(storage.particles + 0x19C0, &end, 4);
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
        const std::span<u8> particles{storage.particles, 0x19C8};
        i32 index = 0;
        do
        {
            const size_t offset = size_t(index) * 0x84 + 0x7C;
            if (i32(read_u32(particles, offset)) == -2)
            {
                std::memcpy(particles.data() + offset, &storage.free_head, 4);
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
            const u32 null = 0;
            std::memcpy(types.data() + index * 0xA0, &null, 4);
        }
        release_animation_resource(registry, storage.types);
        storage.types = {};
    }
}

// Xbox 0x000EB6C0
void set_effect_trail_phase(std::span<u8> particle, i32 phase,
    RuntimeEffectTrails& context)
{
    const auto type = context.types.subspan(size_t(particle[0x71]) * 0xA0, 0xA0);
    particle[0x70] = u8(phase);
    if (phase == 1)
    {
        store_float(particle, 0x74, advance_runtime_random_fraction(context.random) * .25f);
    }
    else if (phase == 2)
    {
        const float fraction = advance_runtime_random_fraction(context.random);
        const auto resource = context.resource(read_u32(type, 0));
        const float extent = float(resource[0x11]) - 1.0f;
        const float complement = 1.0f - fraction;
        for (i32 index = 3; index >= 0; --index)
        {
            const long double value = static_cast<long double>(type[0x88 + index]) *
                complement + static_cast<long double>(type[0x84 + index]) * fraction;
            store_float(particle, 0x10 + size_t(index) * 4,
                float(u8(truncate_runtime_float(value))));
        }
        store_float(particle, 0x20, 255.0f);
        const float duration = read_f32(particle, 0x74);
        store_float(particle, 0x30, 128.0f); // DAT_0032B368 = 0x43000000.
        if (duration > std::bit_cast<float>(0x3456BF95u))
        {
            const float difference = float(ExtendedFloat{128.0f} -
                ExtendedFloat{read_f32(particle, 0x1C)});
            store_float(particle, 0x44,
                float(ExtendedFloat{engine_absolute(difference)} / ExtendedFloat{duration}));
        }
        else
        {
            store_float(particle, 0x1C, 128.0f);
            store_float(particle, 0x44, 0.0f);
        }
        if (extent > 0.0f)
        {
            store_float(particle, 0x74, extent / read_f32(type, 0x9C));
        }
        else
        {
            std::memcpy(particle.data() + 0x74, particle.data() + 0x78, 4);
        }
        store_float(particle, 0x50, read_f32(type, 0x94) * read_f32(particle, 0x4C));
        store_float(particle, 0x54, read_f32(type, 0x98) * read_f32(particle, 0x4C));
        const float rate = (read_f32(particle, 0x54) - read_f32(particle, 0x50)) /
            read_f32(particle, 0x74);
        store_float(particle, 0x58, float(engine_absolute(rate)));
        const auto channel = particle.subspan(0x5C, 0x14);
        for (size_t offset = 0; offset < 0x10; offset += 4)
        {
            store_float(channel, offset, 0.0f);
        }
        channel[0x10] = 0;
        set_runtime_bounded_channel_limits(channel, 0.0f, extent, context.pal, context.override_pal);
        const float frame_rate = read_f32(type, 0x9C);
        store_float(channel, 8, frame_rate);
        const i32 frequency = context.pal != 0 && context.override_pal == 0 ? 50 : 60;
        const float maximum = (float(frequency) * .5f) *
            (read_f32(channel, 4) - read_f32(channel, 0));
        if (frame_rate > maximum)
        {
            store_float(channel, 8, maximum);
        }
        if (read_f32(channel, 0) != read_f32(channel, 4))
        {
            channel[0x10] = 1;
        }
    }
}

// Xbox 0x000EB9F0
void tick_effect_trail(std::span<u8> particle, RuntimeEffectTrails& context,
    const RuntimeActorClock& clock)
{
    const float step = runtime_frame_step(clock);
    approach_actor_effect_channels(particle.subspan(0x10, 0x3C), clock);
    store_float(particle, 0x78, read_f32(particle, 0x78) - step);
    if (particle[0x70] == 1)
    {
        if (read_f32(particle, 0x74) > 0.0f)
        {
            store_float(particle, 0x74, read_f32(particle, 0x74) - step);
            return;
        }
    }
    else if (particle[0x70] == 2)
    {
        if (read_f32(particle, 0x74) > 0.0f)
        {
            store_float(particle, 0x74, read_f32(particle, 0x74) - step);
            float current = read_f32(particle, 0x50);
            float rate = read_f32(particle, 0x58);
            approach_runtime_scalar_rate(current, rate, read_f32(particle, 0x54), step);
            store_float(particle, 0x50, current);
            store_float(particle, 0x58, rate);
            const auto channel = particle.subspan(0x5C, 0x14);
            if ((channel[0x10] & 2) != 0)
            {
                decrease_runtime_bounded_channel(channel, step);
            }
            else if ((channel[0x10] & 1) != 0)
            {
                increase_runtime_bounded_channel(channel, step);
            }
            return;
        }
    }
    else
    {
        return;
    }
    if (read_f32(particle, 0x78) > 0.0f)
    {
        set_effect_trail_phase(particle, 2, context);
    }
    else
    {
        const i32 index = i32((particle.data() - context.particles.data()) / 0x84);
        std::memcpy(context.particles.data() + size_t(index) * 0x84 + 0x7C,
            &context.free_head, 4);
        context.free_head = index;
    }
}
}
