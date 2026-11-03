#include "effect_debris.h"
#include "extended_float.h"
#include "effect_channels.h"
#include "../entities/bounded_channels.h"
#include "../camera/entity_camera.h"
#include "../runtime/random_state.h"
#include "../entities/entity.h"
#include "../entities/entity_heap.h"

#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
float sample_range(const std::array<float, 2>& range, u32& random)
{
    const float lower = range[0];
    const float upper = range[1];
    const float fraction = advance_runtime_random_fraction(random);
    return (upper - lower) * fraction + lower;
}

std::span<const u8> debris_type(std::span<const u8> particle, const RuntimeEffectDebris& context)
{
    return record(context.types, effect_debris_types, get(particle, effect_debris_field::type));
}
}

// Xbox 0x000EC0B0
void tick_effect_debris(std::span<u8> particle, RuntimeEffectDebris& context,
    const RuntimeActorClock& clock)
{
    const float step = runtime_frame_step(clock);
    const auto channels = record(particle, effect_debris_field::channels);
    approach_actor_effect_channels(channels, clock);
    switch (get(particle, effect_debris_field::phase))
    {
    case 1:
    {
        const auto type = debris_type(particle, context);
        set(particle, effect_debris_field::growth, approach_runtime_scalar(get(particle, effect_debris_field::growth),
            get(type, effect_debris_type_field::growth), get(particle, effect_debris_field::growth_rate) * step));
        if (!(get(particle, effect_debris_field::phase_time) > 0.0f))
        {
            set(particle, effect_debris_field::phase, 2);
            set(particle, effect_debris_field::phase_time,
                sample_range(get(type, effect_debris_type_field::hold_time), context.random));
            return;
        }
        break;
    }
    case 2:
        if (!(get(particle, effect_debris_field::phase_time) > 0.0f))
        {
            const auto type = debris_type(particle, context);
            set(particle, effect_debris_field::phase, 3);
            const float duration = sample_range(get(type, effect_debris_type_field::fade_time), context.random);
            set(particle, effect_debris_field::phase_time, duration);
            set(channels, effect_channel_field::target_alpha, 0.0f);
            if (duration > std::bit_cast<float>(0x3456BF95u))
            {
                const float difference = -get(channels, effect_channel_field::alpha);
                set(channels, effect_channel_field::alpha_rate,
                    float(ExtendedFloat{engine_absolute(difference)} / ExtendedFloat{duration}));
            }
            else
            {
                set(channels, effect_channel_field::alpha, 0.0f);
                set(channels, effect_channel_field::alpha_rate, 0.0f);
            }
            return;
        }
        break;
    case 3:
        if (!(get(particle, effect_debris_field::phase_time) > 0.0f))
        {
            const i32 index = i32((particle.data() - context.particles.data()) / effect_debris_field::kSize);
            set(record(context.particles, effect_debris_pool, size_t(index)), effect_debris_field::next_free,
                context.free_head);
            context.free_head = index;
            return;
        }
        break;
    default:
        return;
    }
    set(particle, effect_debris_field::phase_time, get(particle, effect_debris_field::phase_time) - step);
}

// Xbox 0x000EC2D0
void initialize_effect_debris(RuntimeEffectDebrisStorage& storage,
    AnimationResourceRegistry& registry, ResourceErrorState& error,
    const std::function<u32(const u8*)>& address)
{
    storage.particles = static_cast<u8*>(engine_heap_allocate(effect_debris_pool.size));
    storage.free_head = 0;
    const std::span<u8> particles{storage.particles, effect_debris_pool.size};
    i32 index = 0;
    do
    {
        ++index;
        set(record(particles, effect_debris_pool, size_t(index - 1)), effect_debris_field::next_free, index);
    }
    while (index < 49);
    set(record(particles, effect_debris_pool, 49), effect_debris_field::next_free, -1);
    const auto types = acquire_animation_resource(registry, "aid_misc_ghoulies_fx_cheap_splat");
    if (types.data() == nullptr)
    {
        resource_error(error, 2);
    }
    storage.types = types;
    const std::span<u8> mutable_types{const_cast<u8*>(storage.types.data()), storage.types.size()};
    size_t type_index = 0;
    do
    {
        const auto type = record(mutable_types, effect_debris_types, type_index);
        std::span<const u8> texture;
        const char* name = field_data(type, effect_debris_type_field::texture);
        if (std::memcmp(name, "aid_", 4) == 0)
        {
            texture = acquire_animation_resource(registry, name);
            if (texture.data() == nullptr)
            {
                resource_error(error, 2);
            }
        }
        storage.textures[type_index] = texture;
        set(type, effect_debris_type_field::texture_address, address(texture.data()));
        ++type_index;
    }
    while (type_index < effect_debris_types.count);
}

// Xbox 0x000EC3A0
void release_effect_debris(RuntimeEffectDebrisStorage& storage,
    AnimationResourceRegistry& registry)
{
    if (storage.particles != nullptr)
    {
        const std::span<u8> particles{storage.particles, effect_debris_pool.size};
        i32 index = 0;
        do
        {
            const auto particle = record(particles, effect_debris_pool, size_t(index));
            if (get(particle, effect_debris_field::next_free) == -2)
            {
                set(particle, effect_debris_field::next_free, storage.free_head);
                storage.free_head = index;
            }
            ++index;
        }
        while (index < 50);
        // The heap adapter performs the dump's owning-heap selection, temporary
        // DAT_00507808 switch, HeapFree boundary and restoration.
        engine_heap_free(storage.particles);
        storage.particles = nullptr;
    }
    if (storage.types.data() != nullptr)
    {
        const std::span<u8> types{const_cast<u8*>(storage.types.data()), storage.types.size()};
        for (size_t index = 0; index < 10; ++index)
        {
            const auto type = record(types, effect_debris_types, index);
            if (get(type, effect_debris_type_field::texture_address) != 0)
            {
                release_animation_resource(registry, storage.textures[index]);
                set(type, effect_debris_type_field::texture_address, 0);
                storage.textures[index] = {};
            }
        }
        release_animation_resource(registry, storage.types);
        storage.types = {};
    }
}

// Xbox 0x000EBD80
i32 oldest_effect_debris(const RuntimeEffectDebris& context)
{
    u32 oldest = get(record(context.particles, effect_debris_pool, 0), effect_debris_field::frame);
    i32 selected = 0;
    for (i32 index = 1; index < 50; ++index)
    {
        const u32 frame = get(record(context.particles, effect_debris_pool, size_t(index)), effect_debris_field::frame);
        if (frame <= oldest)
        {
            oldest = frame;
            selected = index;
        }
    }
    return selected;
}

// Xbox 0x000EBDB0
std::span<u8> allocate_effect_debris(RuntimeEffectDebris& context)
{
    if (context.free_head >= 0)
    {
        std::span<u8> particle = record(context.particles, effect_debris_pool, size_t(context.free_head));
        const i32 next = get(particle, effect_debris_field::next_free);
        set(particle, effect_debris_field::next_free, -2);
        set(particle, effect_debris_field::phase, 0);
        set(particle, effect_debris_field::phase_time, 0.f);
        set(particle, effect_debris_field::frame, context.frame);
        const auto channels = record(particle, effect_debris_field::channels);
        constexpr float full = 255.f; // DAT_0032B158.
        set(channels, effect_channel_field::red, full);
        set(channels, effect_channel_field::green, full);
        set(channels, effect_channel_field::blue, full);
        set(channels, effect_channel_field::alpha, full);
        set(channels, effect_channel_field::target_red, full);
        set(channels, effect_channel_field::target_green, full);
        set(channels, effect_channel_field::target_blue, full);
        set(channels, effect_channel_field::target_alpha, full);
        set(channels, effect_channel_field::unk_10, 0.f);
        set(channels, effect_channel_field::target_unk_10, 0.f);
        context.free_head = next;
        set(channels, effect_channel_field::rates, std::array<float, 5>{});
        return particle;
    }
    return {};
}

// Xbox 0x000EBE50
void set_effect_debris_phase(std::span<u8> particle, i32 phase,
    RuntimeEffectDebris& context)
{
    const auto type = debris_type(particle, context);
    const auto channels = record(particle, effect_debris_field::channels);
    set(particle, effect_debris_field::phase, u8(phase));
    constexpr float epsilon = std::bit_cast<float>(0x3456BF95u); // DAT_0032B27C.
    if (phase == 1)
    {
        const float duration = sample_range(get(type, effect_debris_type_field::grow_time), context.random);
        set(particle, effect_debris_field::phase_time, duration);
        const float rate = (get(type, effect_debris_type_field::growth) - 1.f) / duration;
        set(particle, effect_debris_field::growth_rate, engine_absolute(rate));
        set(particle, effect_debris_field::growth, 1.f);
        const auto colour = get(type, effect_debris_type_field::colour);
        set(channels, effect_channel_field::red, float(colour[0]));
        set(channels, effect_channel_field::green, float(colour[1]));
        set(channels, effect_channel_field::blue, float(colour[2]));
        set(channels, effect_channel_field::unk_10, 255.f);
        set(channels, effect_channel_field::alpha, 0.f);
        const float alpha = float(colour[3]);
        set(channels, effect_channel_field::target_alpha, alpha);
        if (duration > epsilon)
        {
            const float difference = float(ExtendedFloat{alpha}
                - ExtendedFloat{get(channels, effect_channel_field::alpha)});
            set(channels, effect_channel_field::alpha_rate,
                float(ExtendedFloat{engine_absolute(difference)} / ExtendedFloat{duration}));
            return;
        }
        set(channels, effect_channel_field::alpha, alpha);
        set(channels, effect_channel_field::alpha_rate, 0.f);
    }
    else if (phase == 2)
    {
        set(particle, effect_debris_field::phase_time,
            sample_range(get(type, effect_debris_type_field::hold_time), context.random));
        return;
    }
    else if (phase == 3)
    {
        const float duration = sample_range(get(type, effect_debris_type_field::fade_time), context.random);
        set(particle, effect_debris_field::phase_time, duration);
        set(channels, effect_channel_field::target_alpha, 0.f);
        if (duration > epsilon)
        {
            const float difference = -get(channels, effect_channel_field::alpha);
            set(channels, effect_channel_field::alpha_rate,
                float(ExtendedFloat{engine_absolute(difference)} / ExtendedFloat{duration}));
            return;
        }
        set(channels, effect_channel_field::alpha, 0.f);
        set(channels, effect_channel_field::alpha_rate, 0.f);
        return;
    }
}

// Xbox 0x000EC580
void spawn_effect_debris(i32 type, const Vec3& position, const Vec3& euler,
    RuntimeEffectDebris& context)
{
    std::span<u8> particle = allocate_effect_debris(context);
    if (particle.empty())
    {
        const i32 index = oldest_effect_debris(context);
        set(record(context.particles, effect_debris_pool, size_t(index)), effect_debris_field::next_free,
            context.free_head);
        context.free_head = index;
        particle = allocate_effect_debris(context);
    }
    set(particle, effect_debris_field::position, position);
    set(particle, effect_debris_field::euler, euler);
    set(particle, effect_debris_field::type, u32(type));
    set(particle, effect_debris_field::size, sample_range(
        get(record(context.types, effect_debris_types, size_t(type)), effect_debris_type_field::size), context.random));
    set_effect_debris_phase(particle, 1, context);
}

// Xbox 0x000F1040
void sample_effect_collision_pose(const Vec3& source, const Vec3& normal,
    Vec3& position, Vec3& euler)
{
    position = source;
    const Vec3 direction = normal;
    direction_to_yaw_pitch(direction.x, direction.y, direction.z, euler.y, euler.x);
    euler.z = 0.f;
    constexpr float push = std::bit_cast<float>(0x3D4CCCCDu);
    position.x = direction.x * push + position.x;
    position.y = direction.y * push + position.y;
    position.z = direction.z * push + position.z;
    euler.x += std::bit_cast<float>(0x3FC90FDBu);
}
}
