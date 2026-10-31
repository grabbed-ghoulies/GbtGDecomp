#pragma once
#include "model_tree_dispatch.h"

namespace gbtg
{
struct RuntimeModelBlendshapeDraw
{
    ModelTreeDispatchContext& tree;
    std::span<u32> streams; // Render state +0x78, attribute record addresses.
    std::array<float, 8>& weights; // Render state +0x1B0, the eight strongest target weights.
    // An instance buffer's retail address; its D3D resource (+4) is registered over the buffer's data.
    std::function<u32(RuntimeBlendshapeVertexBuffer&)> buffer_resource;
};
// The instance arrives in EAX.
void select_runtime_model_blend_targets(u32 node, const RuntimeBlendshapeNodeInstance& instance,
    RuntimeModelBlendshapeDraw& context); // Xbox 0x00157880
// With the tree mask's bit 0 (render state +0x44), each position or normal attribute (kind 9 or 10) takes the base
// target's vertices plus each other target's times its weight (+0x1C4), through the attribute's scratch (+0x184) into
// the instance buffer; every attribute's stream (render state +0x78) is then the instance buffer.
void blend_runtime_model_blendshape(u32 node, RuntimeBlendshapeNodeInstance& instance,
    RuntimeModelBlendshapeDraw& context); // Xbox 0x00157650
void dispatch_runtime_model_blendshape(u32 node, RuntimeModelInstanceStorage& storage,
    RuntimeModelBlendshapeDraw& context); // Xbox 0x00157A50
}
