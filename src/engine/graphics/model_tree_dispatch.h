#pragma once
#include "gbtg/model_render.h"
#include <unordered_map>

namespace gbtg
{
struct ModelTreeStackEntry
{
    u32 callback;
    u32 node;
};
struct ModelTreeDispatchContext
{
    RuntimeModelRenderContext& renderer;
    const RuntimeRelocationImage& image;
    std::span<ModelTreeStackEntry> stack; // DAT_004372A8, stride 8
    i32& stack_index; // DAT_0043F4B0
    std::span<RuntimeShaderMatrix> combined; // render frame +0
    std::span<const RuntimeShaderMatrix* const> camera; // render frame +0x40
    u32& retained_204; // render state +0x204
    u16& mask; // render state +0x44
    u32& fog_pixel_state; // DAT_00181CE4
    const u32& cull_mode; // DAT_0032E3D0
    const std::unordered_map<u32, std::function<void(u32, void*)>>& callbacks;
    std::function<void(RuntimeShaderMatrix&, const RuntimeShaderMatrix&, const RuntimeShaderMatrix&)> multiply;
    std::function<void(RuntimeShaderMatrix&, float, float, float)> scaling;
    std::function<void(u32)> set_cull_mode;
    std::function<void(u32)> set_fog_color;
};
void push_runtime_model_node(u32 callback, u32 node, ModelTreeDispatchContext& context); // Xbox 0x00156E40
void pop_runtime_model_node(u32& callback, u32& node, ModelTreeDispatchContext& context); // Xbox 0x00156E70
u32 runtime_model_node_callback(u32 descriptor, const RuntimeRelocationImage& image); // Xbox 0x00158900
void dispatch_runtime_model_nodes(u16 mask, void* storage, ModelTreeDispatchContext& context); // Xbox 0x001589F0
void queue_runtime_model_group(u32 node, ModelTreeDispatchContext& context); // Xbox 0x00158960
void prepare_runtime_model_tree_matrices(ModelTreeDispatchContext& context); // Xbox 0x00158C00
void render_runtime_model_tree(u32 tree, u16 root, u16 mask, void* storage,
    ModelTreeDispatchContext& context); // Xbox 0x00158A80
// The context and its borrowed views must outlive renderer.state.draw.
void install_runtime_model_tree_dispatch(ModelTreeDispatchContext& context);
void install_runtime_model_group_dispatch(ModelTreeDispatchContext& context,
    std::unordered_map<u32, std::function<void(u32, void*)>>& callbacks);
}
