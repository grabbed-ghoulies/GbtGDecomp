#include "model_vertex_shader_draw.h"

namespace gbtg
{
// Xbox 0x00158510
void dispatch_runtime_model_vertex_shader(u32 node, RuntimeModelVertexShaderDraw& context)
{
    auto& transforms = context.transforms;
    auto& tree = transforms.tree;
    runtime_render_state(tree.renderer);
    runtime_render_frame(tree.renderer);
    const auto& image = tree.image;
    u32 flags = runtime_scene_lighting_flags(tree.renderer);
    if (image.get(node, vertex_shader_node::first_channel) == 0xFFFFFFFF)
    {
        flags &= 0x80000000;
    }
    u32 index = 0;
    if (image.get(node, vertex_shader_node::stream_count) != 0)
    {
        do
        {
            const u32 stride = image.byte(image.get(node, vertex_shader_node::strides), index);
            const u32 stream = image.word(image.get(node, vertex_shader_node::streams), index * 4);
            context.stream(index, context.streams[stream] + 4, stride);
            ++index;
        } while (index < image.get(node, vertex_shader_node::stream_count));
    }
    const u32 shader =
        select_runtime_model_vertex_shader(i32(image.get(node, vertex_shader_node::shader)), flags, context.cache);
    context.shader(shader);
    RuntimeShaderMatrix transposed;
    if (image.get(node, vertex_shader_node::projection_register) < 0x60)
    {
        transforms.projection_register = u16(image.get(node, vertex_shader_node::projection_register));
        transforms.world_register = u16(image.get(node, vertex_shader_node::world_register));
        transforms.matrices.transpose(transposed, transforms.projection(tree.renderer.frame_index));
        transforms.constants(u32(transforms.projection_register) + 0x60, transposed.data(), 4);
        if (transforms.world_register < 0x60)
        {
            transforms.matrices.transpose(transposed, tree.combined[tree.renderer.frame_index]);
            transforms.constants(u32(transforms.world_register) + 0x60, transposed.data(), 3);
        }
    }
    if (image.get(node, vertex_shader_node::camera_register) < 0x60)
    {
        transforms.constants(image.get(node, vertex_shader_node::camera_register) + 0x60,
            tree.camera[tree.renderer.frame_index]->data(), 3);
    }
    transforms.constants(image.get(node, vertex_shader_node::parameter_register) + 0x60, context.parameters.data(), 1);
    if (image.get(node, vertex_shader_node::auxiliary_register) < 0x60)
    {
        transforms.constants(image.get(node, vertex_shader_node::auxiliary_register) + 0x60, context.auxiliary.data(),
            2);
    }
    queue_runtime_model_group(node, tree);
}
}
