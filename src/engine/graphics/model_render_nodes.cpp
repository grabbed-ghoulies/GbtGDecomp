#include "model_render_nodes.h"

namespace gbtg
{
// Xbox 0x00158430
void bind_runtime_vertex_streams(u32 node, ModelRenderNodeContext& c)
{
    runtime_render_state(c.tree.renderer);
    const auto& image = c.tree.image;
    u32 index = 0;
    if (image.word(node, 0x24) != 0)
    {
        u32 offset = 0;
        do
        {
            c.stream_descriptors[index] = image.word(node, 0x20) + offset;
            ++index;
            offset += 0x18;
        } while (index < image.word(node, 0x24));
    }
}
// Xbox 0x00158480
void dispatch_runtime_vertex_buffer(u32 node, ModelRenderNodeContext& c)
{
    bind_runtime_vertex_streams(node, c);
    queue_runtime_model_group(node, c.tree);
}
// Xbox 0x00158090
void dispatch_runtime_visibility(u32 node, ModelTreeDispatchContext& c)
{
    queue_runtime_model_group(node, c);
}
// Xbox 0x001586D0
void dispatch_runtime_pixel_shader(u32 node, ModelRenderNodeContext& c)
{
    c.set_pixel_shader(c.pixel_shader);
    c.set_simple_state(0x40354, 0x203);
    c.depth_function = 0x203;
    c.set_depth_enabled(1);
    c.set_simple_state(0x40304, 0);
    c.blend_enabled = 0;
    queue_runtime_model_group(node, c.tree);
}
void install_runtime_render_node_dispatch(ModelRenderNodeContext& c,
    std::unordered_map<u32, std::function<void(u32, void*)>>& callbacks)
{
    callbacks[0x00158480] = [&c](u32 node, void*)
    {
        dispatch_runtime_vertex_buffer(node, c);
    };
    callbacks[0x00158090] = [&c](u32 node, void*)
    {
        dispatch_runtime_visibility(node, c.tree);
    };
    callbacks[0x001586D0] = [&c](u32 node, void*)
    {
        dispatch_runtime_pixel_shader(node, c);
    };
}
}
