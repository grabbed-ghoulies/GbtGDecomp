#pragma once
#include "model_tree_dispatch.h"

namespace gbtg
{
struct ModelRenderNodeContext
{
    ModelTreeDispatchContext& tree;
    std::span<u32> stream_descriptors; // render state +0x78
    const u32& pixel_shader; // DAT_0054D948
    u32& depth_function; // DAT_00181C54
    u32& blend_enabled; // DAT_00181C5C
    std::function<void(u32)> set_pixel_shader; // D3D 0x00175370
    std::function<void(u32, u32)> set_simple_state; // D3D 0x00171050
    std::function<void(u32)> set_depth_enabled; // D3D 0x001722A0
};
void bind_runtime_vertex_streams(u32 node, ModelRenderNodeContext& context); // Xbox 0x00158430
void dispatch_runtime_vertex_buffer(u32 node, ModelRenderNodeContext& context); // Xbox 0x00158480
void dispatch_runtime_visibility(u32 node, ModelTreeDispatchContext& context); // Xbox 0x00158090
void dispatch_runtime_pixel_shader(u32 node, ModelRenderNodeContext& context); // Xbox 0x001586D0
void install_runtime_render_node_dispatch(ModelRenderNodeContext& context,
    std::unordered_map<u32, std::function<void(u32, void*)>>& callbacks);
}
