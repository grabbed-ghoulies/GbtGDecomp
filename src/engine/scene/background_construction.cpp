#include "background_construction.h"
#include "../entities/entity_heap.h"
#include "../entities/actor_collision_culling.h"
#include "../runtime/aid_list_lookup.h"

#include <bit>
#include <cstring>

namespace gbtg
{
// Xbox 0x00042BA0
void runtime_background_storage_sizes(ObjectSizes& output)
{
    output.instance = 0x9E8;
    output.parameters = 0x334;
}

// Xbox 0x00042DA0
bool initialize_runtime_background(RuntimeBackgroundConstruction& state,
    std::span<const u8> parameters, RuntimeBackgroundConstructionServices& services)
{
    const char* name = reinterpret_cast<const char*>(parameters.data() + 0x28);
    size_t character = 0;
    char copied;
    do
    {
        copied = name[character];
        services.room[character] = copied;
        ++character;
    }
    while (copied != 0);
    auto* tail = &services.backgrounds;
    if (tail->next != nullptr)
    {
        tail = static_cast<entity_detail::ListHead*>(tail->next);
        while (tail->next != nullptr)
        {
            tail = static_cast<entity_detail::ListHead*>(tail->next);
        }
    }
    tail->next = &state.lifetime.link;
    state.lifetime.link.prev = tail;
    state.lifetime.link.next = nullptr;
    const auto store = [&](size_t offset, u32 value)
    {
        std::memcpy(state.fields.data() + offset, &value, 4);
    };
    store(0x934, services.address_of(tail));
    store(0x938, 0);
    initialize_background_bounds(state.actor, services.vertices, services.shape_lists);
    initialize_actor_model_matrices(state.actor, state.matrices, services.model_types, services.matrix_bindings);
    const auto cpu = state.actor.model != nullptr ? state.actor.model->cpu : std::span<const u8>{};
    const auto block = [&](u32 kind)
    {
        const u32 count = read_u32(cpu, 4);
        const u32 table = read_u32(cpu, 0);
        for (u32 index = 0; index < count; ++index)
        {
            if (read_u32(cpu, table + index * 8) == kind)
            {
                return read_u32(cpu, table + index * 8 + 4);
            }
        }
        return 0u;
    };
    u32 region = block(0x14);
    do
    {
        if (region == 0)
        {
            break;
        }
        append_background_region(services.regions, services.region_count, cpu.data() + region);
        const u32 next = read_u32(cpu, region);
        if (next == 0)
        {
            break;
        }
        region += next;
    }
    while (true);
    if (block(8) != 0)
    {
        const u32 sector = block(8);
        register_runtime_background_sector({const_cast<u8*>(cpu.data()) + sector, cpu.size() - sector},
            services.room.data(), services.sectors);
    }
    if (state.actor.model != nullptr && cpu.data() != nullptr)
    {
        const u32 runtime = read_u32(cpu, 0x10);
        const u32 header = read_u32(cpu, runtime + 0x10);
        if (header != 0 && cpu[header] != 0)
        {
            const u32 count = cpu[header];
            state.lifetime.volumes = static_cast<u8*>(engine_heap_allocate(i32(count * 0x9C)));
            store(0x94C, services.bind_allocation(state.lifetime.volumes, count * 0x9C));
            std::memset(state.lifetime.volumes, 0, count * 0x9C);
            state.volumes.resize(count);
            for (u32 index = 0; index < count; ++index)
            {
                auto& volume = state.volumes[index];
                volume.fields.fill(0);
                const u32 invalid = 0xFFFFFFFF;
                std::memcpy(volume.fields.data(), &invalid, 4);
                volume.attachment = nullptr;
                volume.anchor = nullptr;
                initialize_background_animation_volume(state.actor, i32(index), cpu.subspan(header), state.volumes,
                    services.attachment_owner, services.attachment_global, services.anchor_owner, services.origin);
                const u32 attachment = services.address_of(volume.attachment);
                const u32 anchor = services.address_of(volume.anchor);
                std::memcpy(volume.fields.data() + 0x14, &attachment, 4);
                std::memcpy(volume.fields.data() + 0x18, &anchor, 4);
                std::memcpy(state.lifetime.volumes + index * 0x9C, volume.fields.data(), 0x9C);
            }
            if (read_u32(cpu, runtime + 0x14) != 0 || read_u32(cpu, runtime + 0x2C) != 0)
            {
                const u32 animations = cpu[header + 1];
                state.lifetime.animation = static_cast<u8*>(engine_heap_allocate(i32(animations * 0x1E0)));
                store(0x950, services.bind_allocation(state.lifetime.animation, animations * 0x1E0));
                state.graphs.reserve(1);
                state.graphs.resize(animations);
                for (u32 index = 0; index < animations; ++index)
                {
                    state.graphs[index].fields = {state.lifetime.animation + index * 0x1E0, 0x1E0};
                }
                state.animation.rows = {state.graphs.data(), animations};
                state.animation.matrix_count = state.matrices.matrices.count;
                state.animation.matrix_node = state.matrices.matrices.node;
                initialize_background_animation(services.first_animation, services.animation_source, services.bind_allocation);
            }
        }
    }
    register_background_doors(cpu, services.doors);
    initialize_background_ambient_nodes(state.actor, services.random, services.ambient);
    if (state.actor.model != nullptr && cpu.data() != nullptr)
    {
        const u32 runtime = read_u32(cpu, 0x10);
        const u32 shapes = read_u32(cpu, runtime + 4);
        if (shapes != 0 && runtime_collision_shape_list_has_physical_shape(state.actor.model->physics_shapes))
        {
            PhysicalShapeEventContext context{0u, 0, 0,
                [](const u8* event, void* user)
                {
                    return collect_background_shape_event(event, *static_cast<GhoulySpawnPool**>(user));
                }, &state.lifetime.events, services.shape_lists};
            visit_model_physical_shape_events(state.actor.model->physics_shapes, context);
            store(0x9E4, services.address_of(state.lifetime.events));
        }
    }
    const auto disabled = acquire_animation_resource(services.resources,
        "aid_aidlist_ghoulies_background_disableframebufferclear");
    if (disabled.data() == nullptr)
    {
        resource_error(services.error, 2);
    }
    const i32 found = find_runtime_aid_list_entry(disabled, reinterpret_cast<const char*>(state.fields.data() + 0x208));
    release_animation_resource(services.resources, disabled);
    if (found != -1)
    {
        services.disable_framebuffer_clear = 1;
        state.lifetime.disables_clear = 1;
        store(0x93C, 1);
    }
    i32 push_buffer = 0;
    if (state.actor.model != nullptr && cpu.data() != nullptr)
    {
        auto* root = state.actor.model->runtime->source->tree->roots[0];
        initialize_background_model_nodes(*root, 0, nullptr, push_buffer, services.nodes);
    }
    state.fields[0x298] = 1;
    state.lifetime.has_push_buffer = push_buffer;
    store(0x9E0, u32(push_buffer));
    store(0x954, 0xFFFFFFFF);
    refresh_actor_model_matrices(state.actor, state.matrices.matrices, 0, services.matrix_boundaries);
    return true;
}
}
