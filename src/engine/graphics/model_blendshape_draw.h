#pragma once
#include "model_tree_dispatch.h"

namespace gbtg
{
struct RuntimeModelBlendshapeDraw
{
    ModelTreeDispatchContext& tree;
    std::span<u32> streams; // Render state +0x78, attribute record addresses.
    std::array<float, 8>& weights; // Render state +0x1B0, the eight strongest target weights.
};
// The instance arrives in EAX.
void select_runtime_model_blend_targets(u32 node, const RuntimeBlendshapeNodeInstance& instance,
    RuntimeModelBlendshapeDraw& context); // Xbox 0x00157880
void dispatch_runtime_model_blendshape(u32 node, RuntimeModelInstanceStorage& storage,
    RuntimeModelBlendshapeDraw& context); // Xbox 0x00157A50
}
