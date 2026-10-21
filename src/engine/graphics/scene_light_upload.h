#pragma once
#include "gbtg/model_render.h"
#include <unordered_map>

namespace gbtg
{
struct SceneLightUploadBoundaries
{
    // Xbox D3D 0x001733E0, D3DX 0x0018462D/0x00184F99/0x00185015.
    std::function<void(u32, const u32*)> set_constant;
    std::function<void(Vec3&, const Vec3&, const float*)> transform_normal;
    std::function<void(Vec3&, const Vec3&)> normalize;
    std::function<void(Vec3&, const Vec3&, const float*)> transform_coord;
};
struct SceneLightUploadContext
{
    RuntimeModelRenderContext& renderer;
    u8& environment_world_space; // DAT_004D9668
    const std::array<float, 4>& retained_scene_color; // DAT_0032E4E4..F0
    std::span<u32, 20> records; // borrowed retail local_50 storage, untouched bytes retained
    const SceneLightUploadBoundaries& boundaries;
};
void store_runtime_vector4(u32* output, u32 x, u32 y, u32 z, u32 w); // Xbox 0x00170250
void upload_runtime_scene_lights(const float* matrix,
    SceneLightUploadContext& context); // Xbox 0x00158F20
}
