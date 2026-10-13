#include "background_lifetime.h"
#include "../entities/ghouly_spawn_record.h"

#include <cstring>

namespace gbtg
{
// Xbox 0x00043D50
void restore_background_shape_event_flags(GhoulySpawnPool*& pool,
    const std::function<void(u8*)>& heap_free)
{
    if (pool != nullptr)
    {
        const u8* end = pool->cursor;
        for (const u8* row = pool->begin; row < end; row += sizeof(BackgroundShapeEvent))
        {
            BackgroundShapeEvent event;
            std::memcpy(&event, row, sizeof(event));
            std::memcpy(const_cast<u8*>(event.source) + 0xC, &event.flags, 4);
        }
        heap_free(reinterpret_cast<u8*>(pool));
        pool = nullptr;
    }
}

// Xbox 0x00044620
void release_background_animation_allocations(u8 count, std::span<BackgroundAnimationAllocation> rows,
    const std::function<void(u8*)>& heap_free)
{
    for (u32 index = 0; index < count; ++index)
    {
        if (rows[index].allocation != nullptr)
        {
            heap_free(rows[index].allocation);
        }
    }
}

// Xbox 0x000430C0
void destroy_runtime_background(RuntimeBackgroundLifetime& background, RuntimeBackgroundRelease& services)
{
    restore_background_shape_event_flags(background.events, services.heap_free);
    if (background.first_push_buffer != nullptr)
    {
        services.heap_free(background.first_push_buffer);
        background.first_push_buffer = nullptr;
    }
    if (background.second_push_buffer != nullptr)
    {
        services.heap_free(background.second_push_buffer);
        background.second_push_buffer = nullptr;
    }
    background.has_push_buffer = 0;
    if (background.effect != nullptr)
    {
        release_actor_effect_node(*background.effect, services.model_sound);
        background.effect = nullptr;
    }
    if (background.volumes != nullptr)
    {
        services.heap_free(background.volumes);
        background.volumes = nullptr;
    }
    if (background.animation != nullptr)
    {
        release_background_animation_allocations(services.first_animation_count,
            services.first_animation_rows, services.heap_free);
        services.heap_free(background.animation);
        background.animation = nullptr;
    }
    if (background.link.prev != nullptr)
    {
        static_cast<entity_detail::ListHead*>(background.link.prev)->next = background.link.next;
    }
    if (background.link.next != nullptr)
    {
        static_cast<entity_detail::ListHead*>(background.link.next)->prev = background.link.prev;
    }
    background.link.prev = nullptr;
    background.link.next = nullptr;
    if (background.disables_clear != 0)
    {
        services.disable_framebuffer_clear = 0;
        background.disables_clear = 0;
    }
}
}
