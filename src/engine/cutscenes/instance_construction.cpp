#include "instance_construction.h"
#include <cstring>
#include <new>

namespace gbtg
{
// Xbox 0x0004D560
RuntimeCutsceneInstance* allocate_cutscene_instance(const u8* resource,
    i32 actor_count, float duration, u32 flags, void* (*allocate)(i32))
{
    void* storage = allocate(static_cast<i32>(sizeof(RuntimeCutsceneInstance))
        + actor_count * 0x30);
    auto* instance = new (storage) RuntimeCutsceneInstance;
    instance->position = {0.f, 0.f, 0.f};
    instance->euler = {0.f, 0.f, 0.f};
    instance->scale = {1.f, 1.f, 1.f};
    instance->duration = duration;
    instance->time = 0.f;
    instance->previous_time = 0.f;
    instance->camera = static_cast<i32>(resource[2]) - 1;
    instance->events = nullptr;
    instance->pending = {0, 0, 0, 0, 0, 0};
    instance->resource = resource;
    instance->actor_count = actor_count;
    instance->flags = flags;
    instance->blend_time = -1.f;
    instance->blend_duration = -1.f;
    instance->actor_rows = reinterpret_cast<u8*>(instance + 1);
    instance->blend_position = {0.f, 0.f, 0.f};
    instance->blend_rotation = {0.f, 0.f, 0.f, 1.f};
    const u32 invalid = 0xFFFFFFFFu;
    const u16 invalid_short = 0xFFFFu;
    std::memcpy(instance->event_state.data(), &invalid, sizeof(invalid));
    std::memcpy(instance->event_state.data() + 4, &invalid_short, sizeof(invalid_short));
    return instance;
}

// Xbox 0x0004DFA0
FactoryObject create_cutscene_actor(ObjectFactoryRegistry& factory,
    std::span<ExecutableResourcePool> pools, const char* parameters,
    const char* model, const Vec3& position, i32 secondary)
{
    auto clone = clone_executable_resource(pools, parameters);
    if (clone.bytes == nullptr)
    {
        return {};
    }
    std::memcpy(clone.bytes.get() + 8, &position.x, sizeof(float));
    std::memcpy(clone.bytes.get() + 0xC, &position.y, sizeof(float));
    std::memcpy(clone.bytes.get() + 0x10, &position.z, sizeof(float));
    if (std::memcmp(model, "aid_", 4) == 0)
    {
        std::strcpy(reinterpret_cast<char*>(clone.bytes.get() + 0x28), model);
    }
    if (secondary != 0)
    {
        const u32 enabled = 1;
        std::memcpy(clone.bytes.get() + 0x2B0, &enabled, sizeof(enabled));
        std::strcpy(reinterpret_cast<char*>(clone.bytes.get() + 0x2B4), model);
    }
    auto object = create_factory_object(factory, {clone.bytes.get(), clone.size});
    clone.bytes.reset(); // 0x000663E0, release the clone's host heap allocation
    return object;
}
}
