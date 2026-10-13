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
    if (image.word(node, 0x3C) == 0xFFFFFFFF)
    {
        flags &= 0x80000000;
    }
    u32 index = 0;
    if (image.word(node, 0x24) != 0)
    {
        do
        {
            const u32 stride = image.byte(image.word(node, 0x58), index);
            const u32 stream = image.word(image.word(node, 0x20), index * 4);
            context.stream(index, context.streams[stream] + 4, stride);
            ++index;
        } while (index < image.word(node, 0x24));
    }
    const u32 shader = select_runtime_model_vertex_shader(i32(image.word(node, 0x64)), flags, context.cache);
    context.shader(shader);
    RuntimeShaderMatrix transposed;
    if (image.word(node, 0x28) < 0x60)
    {
        transforms.projection_register = image.half(node, 0x28);
        transforms.world_register = image.half(node, 0x2C);
        transforms.matrices.transpose(transposed, transforms.projection(tree.renderer.frame_index));
        transforms.constants(u32(transforms.projection_register) + 0x60, transposed.data(), 4);
        if (transforms.world_register < 0x60)
        {
            transforms.matrices.transpose(transposed, tree.combined[tree.renderer.frame_index]);
            transforms.constants(u32(transforms.world_register) + 0x60, transposed.data(), 3);
        }
    }
    if (image.word(node, 0x30) < 0x60)
    {
        transforms.constants(image.word(node, 0x30) + 0x60,
            tree.camera[tree.renderer.frame_index]->data(), 3);
    }
    transforms.constants(image.word(node, 0x34) + 0x60, context.parameters.data(), 1);
    if (image.word(node, 0x4C) < 0x60)
    {
        transforms.constants(image.word(node, 0x4C) + 0x60, context.auxiliary.data(), 2);
    }
    queue_runtime_model_group(node, tree);
}
}
