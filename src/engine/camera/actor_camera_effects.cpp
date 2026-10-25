#include "actor_camera_effects.h"
#include "gbtg/entity.h"

#include <cstring>

namespace gbtg
{
ActorCameraEffect g_actor_camera_effects{};
GhoulySpawnPool* g_actor_camera_shakes = nullptr;

namespace
{
template<class T>
T read(const u8* bytes, size_t offset)
{
    T value;
    std::memcpy(&value, bytes + offset, sizeof(value));
    return value;
}

template<class T>
void write(ActorCameraEffect& effect, size_t offset, const T& value)
{
    std::memcpy(effect.payload + offset - 8, &value, sizeof(value));
}
}

// Xbox 0x00048F10
void release_actor_camera_effects()
{
    while (ActorCameraEffect* effect = g_actor_camera_effects.next)
    {
        if (effect->previous != nullptr)
        {
            effect->previous->next = effect->next;
        }
        if (effect->next != nullptr)
        {
            effect->next->previous = effect->previous;
        }
        effect->previous = nullptr;
        effect->next = nullptr;
        engine_heap_free(effect); // HeapFree on the effect's thread heap
    }
}

// Xbox 0x000FAE40
void release_actor_camera_shakes(AnimationResourceRegistry& registry)
{
    if (GhoulySpawnPool* pool = g_actor_camera_shakes; pool != nullptr)
    {
        for (u8* row = pool->begin; row < pool->cursor; row += pool->stride)
        {
            auto& shake = *reinterpret_cast<ActorCameraShake*>(row);
            if (shake.resource != nullptr)
            {
                release_animation_resource(registry, {shake.resource, 1}); // 0x00035C00, EBX +0x40
                shake.resource = nullptr;
            }
        }
        engine_heap_free(pool); // HeapFree on the pool's thread heap
        g_actor_camera_shakes = nullptr;
    }
    for (u8 port = 0; port < 4; ++port)
    {
        stop_pad_rumble(port); // 0x00155930
    }
}

// Xbox 0x000494B0
ActorCameraEffect* allocate_actor_camera_effect(u8 type)
{
    auto* effect = static_cast<ActorCameraEffect*>(engine_heap_allocate(sizeof(ActorCameraEffect)));
    std::memset(effect, 0, sizeof(*effect));
    ActorCameraEffect* tail = &g_actor_camera_effects;
    if (g_actor_camera_effects.next != nullptr)
    {
        do
        {
            tail = tail->next;
        }
        while (tail->next != nullptr);
    }
    tail->next = effect;
    effect->previous = tail;
    write(*effect, 0x81, type);
    effect->next = nullptr;
    write(*effect, 0x80, u8{1});
    write(*effect, 0x83, u8{0});
    return effect;
}

// Xbox 0x00049520
ActorCameraEffect* create_positioned_camera_effect(u8 type, float size,
    float value, const Vec3& position)
{
    ActorCameraEffect* effect = allocate_actor_camera_effect(type);
    write(*effect, 0x30, size);
    write(*effect, 0x24, value);
    write(*effect, 8, position);
    write(*effect, 0x82, u8{1});
    return effect;
}

// Xbox 0x000493D0
ActorCameraEffect* load_actor_camera_effect(const char* name, const Vec3* position,
    AnimationResourceRegistry& registry, ResourceErrorState& error)
{
    if (name == nullptr || std::memcmp(name, "aid_", 4) != 0)
    {
        return nullptr;
    }
    const auto resource = acquire_animation_resource(registry, name);
    if (resource.data() == nullptr)
    {
        resource_error(error, 2);
        return nullptr;
    }
    const u8* bytes = resource.data();
    ActorCameraEffect* effect;
    if (position != nullptr && read<float>(bytes, 0x10) > 0.f)
    {
        effect = create_positioned_camera_effect(bytes[0], read<float>(bytes, 4),
            read<float>(bytes, 8), *position);
        write(*effect, 0x14, read<u32>(bytes, 0xC));
        write(*effect, 0x18, read<float>(bytes, 0x10));
        release_animation_resource(registry, resource);
        return effect;
    }
    effect = allocate_actor_camera_effect(bytes[0]);
    write(*effect, 0x30, read<float>(bytes, 4));
    write(*effect, 0x24, read<float>(bytes, 8));
    release_animation_resource(registry, resource);
    return effect;
}

// Xbox 0x000FB7C0
ActorCameraShake* allocate_actor_camera_shake()
{
    GhoulySpawnPool* pool = g_actor_camera_shakes;
    if (pool == nullptr)
    {
        return nullptr;
    }
    if (static_cast<u32>((pool->cursor - pool->begin) / pool->stride) >= 16)
    {
        return nullptr;
    }
    if (pool->cursor == pool->limit)
    {
        pool = grow_ghouly_spawn_pool(pool);
        g_actor_camera_shakes = pool;
    }
    auto* shake = reinterpret_cast<ActorCameraShake*>(pool->cursor);
    pool->cursor += pool->stride;
    return shake;
}

// Xbox 0x000FB120
void load_actor_camera_shake(const char* name, const Vec3& position, float scale,
    AnimationResourceRegistry& registry, ResourceErrorState& error)
{
    if (std::memcmp(name, "aid_", 4) == 0)
    {
        const auto resource = acquire_animation_resource(registry, name);
        if (resource.data() == nullptr)
        {
            resource_error(error, 2);
        }
        else
        {
            ActorCameraShake* shake = allocate_actor_camera_shake();
            if (shake != nullptr)
            {
                shake->position = position;
                shake->scale = scale;
                shake->resource = resource.data();
                size_t offset = 0;
                size_t axis = 0;
                do
                {
                    float* values = shake->axes[axis];
                    values[7] = (read<float>(shake->resource, offset + 0x24)
                        - read<float>(shake->resource, offset + 0x20)) * 0.5f
                        + read<float>(shake->resource, offset + 0x20);
                    values[4] = 0.f;
                    values[6] = (read<float>(shake->resource, offset + 0x24)
                        - read<float>(shake->resource, offset + 0x20)) * 0.5f;
                    values[5] = read<float>(shake->resource, offset + 0x2C);
                    values[0] = 0.f;
                    values[1] = read<float>(shake->resource, offset);
                    values[2] = values[1];
                    values[3] = 0.f;
                    offset += 0x3C;
                    ++axis;
                }
                while (offset < 0x78);
                return;
            }
            release_animation_resource(registry, resource);
        }
    }
}
}
