#include "model_tree_dispatch.h"

namespace gbtg
{
// Xbox 0x00156E40
void push_runtime_model_node(u32 callback, u32 node, ModelTreeDispatchContext& c)
{
    ++c.stack_index;
    c.stack[c.stack_index] = ModelTreeStackEntry{callback, node};
}
// Xbox 0x00156E70
void pop_runtime_model_node(u32& callback, u32& node, ModelTreeDispatchContext& c)
{
    callback = c.stack[c.stack_index].callback;
    node = c.stack[c.stack_index].node;
    --c.stack_index;
}
// Xbox 0x00158900
u32 runtime_model_node_callback(u32 descriptor, const RuntimeRelocationImage& image)
{
    return image.word(image.word(descriptor, 0x50));
}
// Xbox 0x001589F0
void dispatch_runtime_model_nodes(u16 mask, void* storage, ModelTreeDispatchContext& c)
{
    u32 callback, node;
    pop_runtime_model_node(callback, node, c);
    while (callback != 0)
    {
        if ((c.image.half(node, 6) & mask) == 0)
        {
            const u32 sibling = c.image.word(node, 0x18);
            if (sibling != 0)
            {
                callback = runtime_model_node_callback(c.image.word(sibling, 0x10), c.image);
                push_runtime_model_node(callback, sibling, c);
            }
        }
        else
        {
            c.callbacks.at(callback)(node, storage);
        }
        pop_runtime_model_node(callback, node, c);
    }
}
// Xbox 0x00158960
void queue_runtime_model_group(u32 node, ModelTreeDispatchContext& c)
{
    const u32 sibling = c.image.word(node, 0x18);
    if (sibling != 0)
    {
        const u32 callback = runtime_model_node_callback(c.image.word(sibling, 0x10), c.image);
        push_runtime_model_node(callback, sibling, c);
    }
    const u32 child = c.image.word(node, 0x14);
    if (child != 0)
    {
        const u32 callback = runtime_model_node_callback(c.image.word(child, 0x10), c.image);
        push_runtime_model_node(callback, child, c);
    }
}
// Xbox 0x00158C00
void prepare_runtime_model_tree_matrices(ModelTreeDispatchContext& c)
{
    RuntimeModelRenderFrame& frame = runtime_render_frame(c.renderer);
    RuntimeShaderMatrix& combined = c.combined[c.renderer.frame_index];
    c.multiply(combined, frame.world, *c.camera[c.renderer.frame_index]);
    RuntimeShaderMatrix scratch = combined;
    std::array<float, 3> scale;
    i32 index = 0;
    do
    {
        const float y = combined[index + 4];
        const float z = combined[index + 8];
        const float x = combined[index];
        const float squared = (z * z + y * y) + x * x;
        scale[index] = static_cast<float>(retail_reciprocal_sqrt(squared));
        ++index;
    } while (index < 3);
    c.scaling(scratch, scale[0], scale[1], scale[2]);
    c.multiply(scratch, combined, scratch);
}
// Xbox 0x00158A80
void render_runtime_model_tree(u32 tree, u16 root, u16 mask, void* storage,
    ModelTreeDispatchContext& c)
{
    prepare_runtime_model_tree_matrices(c);
    RuntimeModelRenderState& state = runtime_render_state(c.renderer);
    c.retained_204 = 0;
    c.set_cull_mode(c.cull_mode);
    c.set_fog_color(state.fog_color);
    c.renderer.fog_render_state = state.fog_enabled;
    c.renderer.changed_render_states |= 0x2000;
    c.fog_pixel_state = 0;
    c.mask = mask;
    push_runtime_model_node(0, 0, c);
    const u32 node = c.image.word(c.image.word(tree, 8), static_cast<u32>(root) * 4);
    const u32 callback = runtime_model_node_callback(c.image.word(node, 0x10), c.image);
    push_runtime_model_node(callback, node, c);
    dispatch_runtime_model_nodes(c.mask, storage, c);
}
void install_runtime_model_tree_dispatch(ModelTreeDispatchContext& c)
{
    c.renderer.state.draw = [&c](u32 tree, u32 root, u32 mask, void* storage)
    {
        render_runtime_model_tree(tree, static_cast<u16>(root), static_cast<u16>(mask), storage, c);
    };
}
void install_runtime_model_group_dispatch(ModelTreeDispatchContext& c,
    std::unordered_map<u32, std::function<void(u32, void*)>>& callbacks)
{
    callbacks[0x00158960] = [&c](u32 node, void*)
    {
        queue_runtime_model_group(node, c);
    };
}
}
