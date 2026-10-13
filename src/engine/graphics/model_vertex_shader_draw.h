#pragma once
#include "model_vertex_shader_cache.h"
#include "model_transform_draw.h"

namespace gbtg
{
struct RuntimeModelVertexShaderDraw
{
    RuntimeModelTransformDraw& transforms;
    RuntimeModelVertexShaderCacheContext& cache;
    std::span<const u32> streams; // Render state +0x78.
    const std::array<float, 4>& parameters; // Render state +0x198.
    const std::array<float, 8>& auxiliary; // Render state +0x1B0.
    std::function<void(u32, u32, u32)> stream; // D3D 0x00173610.
    std::function<void(u32)> shader; // D3D 0x001739F0.
};
void dispatch_runtime_model_vertex_shader(u32 node, RuntimeModelVertexShaderDraw& context); // Xbox 0x00158510
}
