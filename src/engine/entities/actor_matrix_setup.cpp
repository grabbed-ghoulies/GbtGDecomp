#include "actor_matrix_setup.h"

#include <cstring>

namespace gbtg
{
// Xbox 0x00155FE0
i32 runtime_matrix_array_count(std::span<const u8> node)
{
    return i32(read_u32(node, 0x30));
}

// Xbox 0x00155FF0
u32 runtime_matrix_array_matrix_offset(std::span<const u8> node, i32 index)
{
    return read_u32(node, 0x2C) + (u32(index) << 6);
}

// Xbox 0x00040BE0
u32 actor_model_link_head_offset(const Actor& actor)
{
    if (actor.model != nullptr && actor.model->cpu.data() != nullptr)
    {
        const auto cpu = actor.model->cpu;
        return read_u32(cpu, read_u32(cpu, 0x10) + 8);
    }
    return 0;
}

// Xbox 0x00053000
u32 runtime_model_animation_matrix_array(std::span<const u8> cpu)
{
    const u32 count = read_u32(cpu, 4);
    const u32 table = read_u32(cpu, 0);
    for (u32 index = 0; index < count; ++index)
    {
        if (read_u32(cpu, table + index * 8) == 0x11)
        {
            const u32 block = read_u32(cpu, table + index * 8 + 4);
            if (block == 0)
            {
                return 0;
            }
            return read_u32(cpu, block + 8);
        }
    }
    return 0;
}

// Xbox 0x00156050
u32 runtime_matrix_array_binding(std::span<const u8> node, std::span<const u8> bindings,
    u32 index)
{
    i32 current = 0;
    if (0 < i32(read_u32(node, 0x24)))
    {
        do
        {
            if (index == bindings[current * 8 + 6])
            {
                if ((bindings[current * 8 + 5] & 1) != 0)
                {
                    return 0xFFFFFFFFu;
                }
                return read_u16(bindings, current * 8);
            }
            ++current;
        }
        while (current < i32(read_u32(node, 0x24)));
    }
    return 0xFFFFFFFFu;
}

// Xbox 0x000400D0
bool initialize_actor_model_matrices(const Actor& actor, ActorMatrixSetup& state,
    std::span<const RuntimeModelNodeType> types, const ActorMatrixBindings& bindings)
{
    if (actor.model != nullptr && actor.model->cpu.data() != nullptr)
    {
        // Native view of model CPU +0x10 -> +0x10 (directory block 16).
        const auto& asset = *actor.model->runtime->source;
        const u8* animation_header = nullptr;
        for (const auto& block : asset.blocks)
        {
            if (block.type == 16)
            {
                animation_header = block.data;
                break;
            }
        }
        if (animation_header != nullptr)
        {
            state.matrices.instance = actor.model->runtime->tree_storage.bytes;
            auto* node = find_runtime_model_node_by_name(*asset.tree, 0, "ndMtxArray", types);
            state.matrices.node = node != nullptr ? node->bytes : std::span<const u8>{};
            if (node != nullptr)
            {
                state.matrices.count = runtime_matrix_array_count(node->bytes);
                state.allocation = static_cast<u8*>(engine_heap_allocate(u32(state.matrices.count) * 0x48u));
                state.matrices.records = {state.allocation, u32(state.matrices.count) * 0x48u};
                i32 index = 0;
                if (0 < state.matrices.count)
                {
                    do
                    {
                        auto* record = state.allocation + index * 0x48;
                        auto store = [&](size_t offset, u32 value)
                        {
                            std::memcpy(record + offset, &value, sizeof(value));
                        };
                        store(0xC, 0);
                        store(8, 0);
                        store(4, 0);
                        store(0x18, 0);
                        store(0x14, 0);
                        store(0x10, 0);
                        store(0x1C, 0x3F800000u);
                        store(0x20, 0x3F800000u);
                        store(0x24, 0x3F800000u);
                        store(0x30, 0);
                        store(0x2C, 0);
                        store(0x28, 0);
                        state.progress = 0.f;
                        state.flags = 0;
                        store(0x44, 0);
                        const auto view = i32(read_u32(node->bytes, 0x24)) > 0 ?
                            bindings.at(node->bytes.data()) : std::span<const u8>{};
                        store(0, runtime_matrix_array_binding(node->bytes, view, u32(index)));
                        store(0x34, 0);
                        ++index;
                    }
                    while (index < state.matrices.count);
                }
                return true;
            }
        }
    }
    return false;
}
}
