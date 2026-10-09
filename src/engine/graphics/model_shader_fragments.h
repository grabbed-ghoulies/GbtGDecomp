#pragma once
#include "gbtg/model_render.h"
#include <unordered_map>

namespace gbtg
{
struct RuntimeShaderFragmentContext
{
    RuntimeModelRenderContext& renderer;
    std::span<RuntimeRenderLight, 239> buckets; // DAT_004439A8, stride 8 sentinel links
    const std::unordered_map<RuntimeRenderLight*, u32>& programs; // actual node +0xC addresses; type holds +8 key
};
u32 runtime_scene_lighting_flags(const RuntimeModelRenderContext& renderer); // Xbox 0x00159390
void find_runtime_shader_fragment(u32& program, u8 use_lighting,
    RuntimeShaderFragmentContext& context); // Xbox 0x00158EB0
}
