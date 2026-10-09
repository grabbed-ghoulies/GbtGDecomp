#pragma once
#include "model_tree_dispatch.h"

namespace gbtg
{
struct ModelPushBufferDrawContext
{
    ModelTreeDispatchContext& tree;
    // Actual render state +0x1F8 flag origins; preserve signed reads around the origin.
    const std::unordered_map<RuntimeModelInstance*, const u8*>& flag_origins;
    std::function<void(u32, u32, u32)> draw_indexed; // D3D 0x001729C0
};
bool runtime_push_buffer_visible(u32 node, ModelPushBufferDrawContext& context); // Xbox 0x00158240
void draw_runtime_push_buffer(u32 node, ModelPushBufferDrawContext& context); // Xbox 0x001582A0
void install_runtime_push_buffer_dispatch(ModelPushBufferDrawContext& context,
    std::unordered_map<u32, std::function<void(u32, void*)>>& callbacks);
}
