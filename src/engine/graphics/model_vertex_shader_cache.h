#pragma once
#include "gbtg/model_relocation.h"
#include "model_shader_fragments.h"

namespace gbtg
{
struct RuntimeModelVertexShaderCacheContext
{
    RuntimeVertexShaderCache& cache;
    const RuntimeRelocationImage& image;
    RuntimeShaderFragmentContext& fragments;
    u32& fragment_word; // Borrowed retained caller word for fragment lookup.
    std::function<void(const i32*, const u32*, u32&, u32)> create; // D3D 0x001732A0.
};
u32 create_runtime_model_vertex_shader(RuntimeVertexShaderCacheEntry& entry, u32 flags,
    RuntimeModelVertexShaderCacheContext& context); // Xbox 0x00159530
u32 select_runtime_model_vertex_shader(i32 entry, u32 flags,
    RuntimeModelVertexShaderCacheContext& context); // Xbox 0x001596D0
}
