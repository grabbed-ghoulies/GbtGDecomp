#pragma once
#include "model_tree_dispatch.h"

namespace gbtg
{
struct RuntimeModelSkeletonDraw
{
    ModelTreeDispatchContext& tree;
    const std::function<ModelPoseBuffer*(void*, u32)>& pose; // Native skeleton instance pointer word.
};
void dispatch_runtime_model_skeleton(u32 node, void* storage,
    RuntimeModelSkeletonDraw& context); // Xbox 0x001569D0
}
