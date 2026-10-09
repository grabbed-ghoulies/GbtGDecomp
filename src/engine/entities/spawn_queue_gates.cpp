#include "spawn_queue_gates.h"
#include "actor_lookup.h"

#include <bit>
#include <cstring>

namespace gbtg
{
// Xbox 0x0013E7E0
bool spawn_queue_below_minimum(const GhoulySpawnContext& context)
{
    const i32 deficit = std::bit_cast<i32>(u32(context.minimum) - u32(context.body_count));
    if ((deficit & (deficit < 1 ? 0 : -1)) != 0)
    {
        i32 pending = 0;
        for (auto* record = g_side_list.next; record != nullptr; record = record->next)
        {
            if ((record->flags_2c & 1) == 0)
            {
                ++pending;
            }
        }
        return pending == 0;
    }
    return false;
}

// Xbox 0x0013E790
bool spawn_queue_below_maximum(const GhoulySpawnContext& context)
{
    const i32 capacity = std::bit_cast<i32>(u32(context.maximum) - u32(context.body_count));
    if (context.maximum != 0 && (capacity & (capacity < 1 ? 0 : -1)) == 0)
    {
        return false;
    }
    i32 pending = 0;
    for (auto* record = g_side_list.next; record != nullptr; record = record->next)
    {
        if ((record->flags_2c & 1) == 0)
        {
            ++pending;
        }
    }
    return pending == 0;
}

// Xbox 0x00051900
bool spawn_script_under_class_limits(const u8* records, const GhoulySpawnContext& context)
{
    while (true)
    {
        u32 kind;
        std::memcpy(&kind, records + 4, sizeof(kind));
        if (kind == 0)
        {
            return true;
        }
        if (kind == 1 && !spawn_resource_under_class_limit(context.resources,
            context.class_limits, reinterpret_cast<const char*>(records + 8)))
        {
            return false;
        }
        u32 stride;
        std::memcpy(&stride, records, sizeof(stride));
        records += stride;
    }
}

// Xbox 0x0013EB10
bool spawn_box_under_class_limits(const char* name, AnimationResourceRegistry& registry,
    ResourceErrorState& error, const GhoulySpawnContext& context)
{
    if (std::memcmp(name, "aid_", 4) == 0)
    {
        auto resource = acquire_animation_resource(registry, name);
        if (resource.data() == nullptr)
        {
            resource_error(error, 2);
            return false;
        }
        const bool result = spawn_script_under_class_limits(resource.data(), context);
        release_animation_resource(registry, resource);
        return result;
    }
    return false;
}

// Xbox 0x000F5200
bool tick_spawn_queue_interval(SpawnPoolSelection& selection, float& timer, float delta,
    const char* name, AnimationResourceRegistry& registry, ResourceErrorState& error,
    GhoulySpawnContext& context)
{
    if (timer <= 0.f)
    {
        timer += select_spawn_interval(selection, context.random_state);
    }
    timer -= delta;
    if (spawn_queue_below_minimum(context))
    {
        timer = 0.f;
    }
    else if (timer <= 0.f)
    {
        if (!spawn_queue_below_maximum(context) ||
            !spawn_box_under_class_limits(name, registry, error, context))
        {
            timer = std::bit_cast<float>(0x3A83126Fu);
        }
    }
    return timer <= 0.f;
}
}
