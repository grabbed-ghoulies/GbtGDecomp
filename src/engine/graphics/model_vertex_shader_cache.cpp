#include "model_vertex_shader_cache.h"
#include "shader_optimization_driver.h"
#include <bit>

namespace gbtg
{
// Xbox 0x00159530
u32 create_runtime_model_vertex_shader(RuntimeVertexShaderCacheEntry& entry, u32 flags,
    RuntimeModelVertexShaderCacheContext& context)
{
    if (entry.variant_count == 6)
    {
        return std::bit_cast<u32>(entry.declaration[42]);
    }
    entry.declaration[42] = entry.declaration[40];
    entry.declaration[43] = entry.declaration[41];
    entry.declaration[40] = entry.declaration[38];
    entry.declaration[41] = entry.declaration[39];
    entry.declaration[38] = entry.declaration[36];
    entry.declaration[39] = entry.declaration[37];
    entry.declaration[36] = entry.declaration[34];
    entry.declaration[37] = entry.declaration[35];
    entry.declaration[34] = entry.declaration[32];
    entry.declaration[35] = entry.declaration[33];
    std::array<const u32*, 3> sources;
    sources[0] = entry.program.data();
    u32 count;
    if (entry.first_channel == 0)
    {
        count = 2;
        if (std::bit_cast<i32>(flags) < 0)
        {
            sources[1] = reinterpret_cast<const u32*>(context.image.pointer(0x0032E65C));
        }
        else
        {
            sources[1] = reinterpret_cast<const u32*>(context.image.pointer(0x0032E690));
        }
    }
    else
    {
        find_runtime_shader_fragment(context.fragment_word, u8(entry.second_channel), context.fragments);
        sources[1] = reinterpret_cast<const u32*>(context.image.pointer(context.fragment_word));
        u32 fragment;
        switch (entry.key)
        {
        case 0:
            fragment = 0x0032E58C;
            break;
        case 1:
            fragment = 0x0032E5C0;
            break;
        case 2:
            fragment = 0x0032E5F4;
            break;
        case 3:
            fragment = 0x0032E628;
            break;
        default:
            fragment = 0x0032E558;
        }
        sources[2] = reinterpret_cast<const u32*>(context.image.pointer(fragment));
        count = 3;
    }
    std::array<u32, 545> program;
    u32 size = 0x884;
    splice_runtime_vertex_shaders(program.data(), &size, nullptr, sources.data(), count, 0, nullptr);
    context.create(entry.declaration.data(), program.data(), reinterpret_cast<u32&>(entry.declaration[32]), 0);
    const i32 next = std::bit_cast<i32>(std::bit_cast<u32>(entry.variant_count) + 1);
    entry.declaration[33] = std::bit_cast<i32>(flags);
    entry.variant_count = next;
    if (next >= 6)
    {
        entry.variant_count = 6;
    }
    return std::bit_cast<u32>(entry.declaration[32]);
}

// Xbox 0x001596D0
u32 select_runtime_model_vertex_shader(i32 index, u32 flags, RuntimeModelVertexShaderCacheContext& context)
{
    auto& entry = context.cache.entries[index];
    const i32 count = entry.variant_count;
    i32 slot = 0;
    if (count > 0)
    {
        do
        {
            if (std::bit_cast<u32>(entry.declaration[33 + slot * 2]) == flags)
            {
                return std::bit_cast<u32>(entry.declaration[32 + slot * 2]);
            }
            ++slot;
        } while (slot < count);
    }
    return create_runtime_model_vertex_shader(entry, flags, context);
}
}
