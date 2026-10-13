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
void store_word(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}
void store_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}
float sample_range(std::span<const u8> type, size_t offset, u32& random)
{
    const float lower = read_f32(type, offset);
    const float upper = read_f32(type, offset + 4);
    const float fraction = advance_runtime_random_fraction(random);
    return (upper - lower) * fraction + lower;
}
}

// Xbox 0x000EC0B0
void tick_effect_debris(std::span<u8> particle, RuntimeEffectDebris& context,
    const RuntimeActorClock& clock)
{
    const float step = runtime_frame_step(clock);
    approach_actor_effect_channels(particle.subspan(0x24, 0x3C), clock);
    switch (particle[0x60])
    {
    case 1:
    {
        const auto type = context.types.subspan(size_t(read_u32(particle, 0x6C)) * 0xAC, 0xAC);
        store_float(particle, 0x20, approach_runtime_scalar(read_f32(particle, 0x20),
            read_f32(type, 0x90), read_f32(particle, 0x1C) * step));
        if (!(read_f32(particle, 0x64) > 0.0f))
        {
            particle[0x60] = 2;
            store_float(particle, 0x64, sample_range(type, 0x9C, context.random));
            return;
        }
        break;
    }
    case 2:
        if (!(read_f32(particle, 0x64) > 0.0f))
        {
            const auto type = context.types.subspan(size_t(read_u32(particle, 0x6C)) * 0xAC, 0xAC);
            particle[0x60] = 3;
            const float duration = sample_range(type, 0xA4, context.random);
            store_float(particle, 0x64, duration);
            store_float(particle, 0x44, 0.0f);
            if (duration > std::bit_cast<float>(0x3456BF95u))
            {
                const float difference = -read_f32(particle, 0x30);
                store_float(particle, 0x58,
                    float(ExtendedFloat{engine_absolute(difference)} / ExtendedFloat{duration}));
            }
            else
            {
                store_float(particle, 0x30, 0.0f);
                store_float(particle, 0x58, 0.0f);
            }
            return;
        }
        break;
    case 3:
        if (!(read_f32(particle, 0x64) > 0.0f))
        {
            const i32 index = i32((particle.data() - context.particles.data()) / 0x74);
            store_word(context.particles, size_t(index) * 0x74 + 0x68, u32(context.free_head));
            context.free_head = index;
            return;
        }
        break;
    default:
        return;
    }
    store_float(particle, 0x64, read_f32(particle, 0x64) - step);
}

// Xbox 0x000EC2D0
void initialize_effect_debris(RuntimeEffectDebrisStorage& storage,
    AnimationResourceRegistry& registry, ResourceErrorState& error,
    const std::function<u32(const u8*)>& address)
{
    storage.particles = static_cast<u8*>(engine_heap_allocate(0x16A8));
    storage.free_head = 0;
    const std::span<u8> particles{storage.particles, 0x16A8};
    i32 index = 0;
    do
    {
        ++index;
        store_word(particles, size_t(index - 1) * 0x74 + 0x68, u32(index));
    }
    while (index < 49);
    store_word(particles, 0x169C, 0xFFFFFFFFu);
    const auto types = acquire_animation_resource(registry, "aid_misc_ghoulies_fx_cheap_splat");
    if (types.data() == nullptr)
    {
        resource_error(error, 2);
    }
    storage.types = types;
    const std::span<u8> mutable_types{const_cast<u8*>(storage.types.data()), storage.types.size()};
    size_t offset = 0;
    do
    {
        std::span<const u8> texture;
        if (read_u32(storage.types, offset) == 0x5F646961u)
        {
            texture = acquire_animation_resource(registry,
                reinterpret_cast<const char*>(storage.types.data() + offset));
            if (texture.data() == nullptr)
            {
                resource_error(error, 2);
            }
        }
        storage.textures[offset / 0xAC] = texture;
        store_word(mutable_types, offset + 0x80, address(texture.data()));
        offset += 0xAC;
    }
    while (offset < 0x6B8);
}

// Xbox 0x000EC3A0
void release_effect_debris(RuntimeEffectDebrisStorage& storage,
    AnimationResourceRegistry& registry)
{
    if (storage.particles != nullptr)
    {
        const std::span<u8> particles{storage.particles, 0x16A8};
        i32 index = 0;
        do
        {
            const size_t offset = size_t(index) * 0x74 + 0x68;
            if (i32(read_u32(particles, offset)) == -2)
            {
                store_word(particles, offset, u32(storage.free_head));
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
            const size_t offset = index * 0xAC + 0x80;
            if (read_u32(types, offset) != 0)
            {
                release_animation_resource(registry, storage.textures[index]);
                store_word(types, offset, 0);
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
    u32 oldest = read_u32(context.particles, 0x70);
    i32 selected = 0;
    for (i32 index = 1; index < 50; ++index)
    {
        const u32 frame = read_u32(context.particles, size_t(index) * 0x74 + 0x70);
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
        std::span<u8> particle = context.particles.subspan(size_t(context.free_head) * 0x74, 0x74);
        const i32 next = i32(read_u32(particle, 0x68));
        store_word(particle, 0x68, 0xFFFFFFFEu);
        particle[0x60] = 0;
        store_word(particle, 0x64, 0);
        store_word(particle, 0x70, context.frame);
        for (size_t offset : {0x24, 0x28, 0x2C, 0x30, 0x38, 0x3C, 0x40, 0x44})
        {
            store_float(particle, offset, 255.f); // DAT_0032B158.
        }
        store_word(particle, 0x34, 0);
        store_word(particle, 0x48, 0);
        context.free_head = next;
        for (size_t offset = 0x4C; offset <= 0x5C; offset += 4)
        {
            store_word(particle, offset, 0);
        }
        return particle;
    }
    return {};
}

// Xbox 0x000EBE50
void set_effect_debris_phase(std::span<u8> particle, i32 phase,
    RuntimeEffectDebris& context)
{
    const auto type = context.types.subspan(size_t(read_u32(particle, 0x6C)) * 0xAC, 0xAC);
    particle[0x60] = u8(phase);
    constexpr float epsilon = std::bit_cast<float>(0x3456BF95u); // DAT_0032B27C.
    if (phase == 1)
    {
        const float duration = sample_range(type, 0x94, context.random);
        store_float(particle, 0x64, duration);
        const float rate = (read_f32(type, 0x90) - 1.f) / duration;
        store_float(particle, 0x1C, engine_absolute(rate));
        store_float(particle, 0x20, 1.f);
        store_float(particle, 0x24, float(type[0x84]));
        store_float(particle, 0x28, float(type[0x85]));
        store_float(particle, 0x2C, float(type[0x86]));
        store_float(particle, 0x34, 255.f);
        store_float(particle, 0x30, 0.f);
        const float alpha = float(type[0x87]);
        store_float(particle, 0x44, alpha);
        if (duration > epsilon)
        {
            const float difference = float(ExtendedFloat{alpha} - ExtendedFloat{read_f32(particle, 0x30)});
            store_float(particle, 0x58, float(ExtendedFloat{engine_absolute(difference)} / ExtendedFloat{duration}));
            return;
        }
        store_float(particle, 0x30, alpha);
        store_float(particle, 0x58, 0.f);
    }
    else if (phase == 2)
    {
        store_float(particle, 0x64, sample_range(type, 0x9C, context.random));
        return;
    }
    else if (phase == 3)
    {
        const float duration = sample_range(type, 0xA4, context.random);
        store_float(particle, 0x64, duration);
        store_float(particle, 0x44, 0.f);
        if (duration > epsilon)
        {
            const float difference = -read_f32(particle, 0x30);
            store_float(particle, 0x58, float(ExtendedFloat{engine_absolute(difference)} / ExtendedFloat{duration}));
            return;
        }
        store_float(particle, 0x30, 0.f);
        store_float(particle, 0x58, 0.f);
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
        store_word(context.particles, size_t(index) * 0x74 + 0x68, u32(context.free_head));
        context.free_head = index;
        particle = allocate_effect_debris(context);
    }
    std::memcpy(particle.data(), &position, 12);
    std::memcpy(particle.data() + 0xC, &euler, 12);
    store_word(particle, 0x6C, u32(type));
    store_float(particle, 0x18, sample_range(context.types.subspan(size_t(type) * 0xAC, 0xAC),
        0x88, context.random));
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
