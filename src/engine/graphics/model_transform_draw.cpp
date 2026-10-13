#include "model_transform_draw.h"
#include "d3dx_matrix.h"
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
void queue_transform_children(u32 node, u32 restore, ModelTreeDispatchContext& tree)
{
    const auto& image = tree.image;
    const u32 sibling = image.word(node, 0x18);
    if (sibling != 0)
    {
        const u32 callback = runtime_model_node_callback(image.word(sibling, 0x10), image);
        push_runtime_model_node(callback, image.word(node, 0x18), tree);
    }
    push_runtime_model_node(restore, node, tree);
    const u32 child = image.word(node, 0x14);
    if (child != 0)
    {
        const u32 callback = runtime_model_node_callback(image.word(child, 0x10), image);
        push_runtime_model_node(callback, image.word(node, 0x14), tree);
    }
}
}

// Xbox 0x00156E00
void push_runtime_model_frame(RuntimeModelTransformDraw& context)
{
    auto& renderer = context.tree.renderer;
    const i32 index = renderer.frame_index;
    context.tree.combined[index + 1] = context.tree.combined[index];
    context.cameras[index + 1] = context.cameras[index];
    renderer.frames[index + 1].world = renderer.frames[index].world;
    context.projection(index + 1) = context.projection(index);
    renderer.frame_index = i32(u32(index) + 1);
}

// Xbox 0x00156E30
void pop_runtime_model_frame(RuntimeModelRenderContext& renderer)
{
    renderer.frame_index = i32(u32(renderer.frame_index) - 1);
}

// Xbox 0x00156A30
void translate_runtime_model_joint_matrix(u32 joint, const RuntimeShaderMatrix& input,
    RuntimeShaderMatrix& output, const RuntimeRelocationImage& image)
{
    if (&input != &output)
    {
        output = input;
    }
    const float x = std::bit_cast<float>(image.word(joint, 0x10));
    const float y = std::bit_cast<float>(image.word(joint, 0x14));
    const float z = std::bit_cast<float>(image.word(joint, 0x18));
    output[12] = ((input[8] * z + input[4] * y) + x * input[0]) + output[12];
    output[13] = ((input[9] * z + input[1] * x) + input[5] * y) + output[13];
    output[14] = ((input[10] * z + input[2] * x) + input[6] * y) + output[14];
}

// Xbox 0x00156540
void upload_runtime_model_frame_matrix(const RuntimeShaderMatrix& matrix,
    RuntimeModelTransformDraw& context)
{
    runtime_render_frame(context.tree.renderer);
    runtime_render_state(context.tree.renderer);
    RuntimeShaderMatrix transposed;
    context.matrices.transpose(transposed, context.projection(context.tree.renderer.frame_index));
    context.constants(u32(context.projection_register) + 0x60, transposed.data(), 4);
    if (context.world_register < 0x60)
    {
        context.matrices.transpose(transposed, matrix);
        context.constants(u32(context.world_register) + 0x60, transposed.data(), 3);
    }
}

// Xbox 0x00157F10
void apply_runtime_model_node_transform(u32 node, std::span<const u8> storage,
    RuntimeModelTransformDraw& context)
{
    const auto& image = context.tree.image;
    const u32 offset = image.word(node, 0xC);
    auto& frame = runtime_render_frame(context.tree.renderer);
    auto& state = runtime_render_state(context.tree.renderer);
    RuntimeShaderMatrix matrix;
    if ((image.byte(node, 0x20) & 0xF) == 2)
    {
        const u32 joint = image.half(node, 0x22);
        translate_runtime_model_joint_matrix(state.skeleton_matrices + joint * 0x20,
            state.skeleton_pose->packed_matrices[joint], matrix, image);
    }
    else
    {
        const u8* source;
        if ((storage[offset + 4] & 1) == 0)
        {
            source = image.pointer(node + 0x24);
        }
        else
        {
            source = storage.data() + offset + 8;
        }
        std::memcpy(matrix.data(), source, 0x40);
    }
    multiply_runtime_graphics_matrices(frame.world.data(), matrix.data(), frame.world.data());
    auto& combined = context.tree.combined[context.tree.renderer.frame_index];
    multiply_runtime_graphics_matrices(combined.data(), frame.world.data(),
        context.cameras[context.tree.renderer.frame_index]->data());
    upload_runtime_model_frame_matrix(combined, context);
}

// Xbox 0x00157ED0
void restore_runtime_model_transform(RuntimeModelTransformDraw& context)
{
    pop_runtime_model_frame(context.tree.renderer);
    auto& frame = runtime_render_frame(context.tree.renderer);
    auto& combined = context.tree.combined[context.tree.renderer.frame_index];
    multiply_runtime_graphics_matrices(combined.data(), frame.world.data(),
        context.cameras[context.tree.renderer.frame_index]->data());
    upload_runtime_model_frame_matrix(combined, context);
}

// Xbox 0x00157D30
void restore_runtime_model_billboard(RuntimeModelTransformDraw& context)
{
    pop_runtime_model_frame(context.tree.renderer);
    runtime_render_frame(context.tree.renderer);
    upload_runtime_model_frame_matrix(context.tree.combined[context.tree.renderer.frame_index], context);
}

// Xbox 0x00157FC0
void dispatch_runtime_model_transform(u32 node, std::span<const u8> storage,
    RuntimeModelTransformDraw& context)
{
    push_runtime_model_frame(context);
    apply_runtime_model_node_transform(node, storage, context);
    queue_transform_children(node, 0x00157ED0, context.tree);
}

// Xbox 0x00157D50
void dispatch_runtime_model_billboard(u32 node, std::span<const u8> storage,
    RuntimeModelTransformDraw& context)
{
    push_runtime_model_frame(context);
    apply_runtime_model_node_transform(node, storage, context);
    auto& frame = runtime_render_frame(context.tree.renderer);
    auto& combined = context.tree.combined[context.tree.renderer.frame_index];
    combined = frame.world;
    const auto& camera = *context.cameras[context.tree.renderer.frame_index];
    combined[12] = ((camera[8] * frame.world[14] + camera[4] * frame.world[13]) +
        frame.world[12] * camera[0]) + camera[12];
    combined[13] = ((camera[9] * frame.world[14] + camera[5] * frame.world[13]) +
        camera[1] * frame.world[12]) + camera[13];
    combined[14] = ((camera[10] * frame.world[14] + camera[6] * frame.world[13]) +
        camera[2] * frame.world[12]) + camera[14];
    upload_runtime_model_frame_matrix(combined, context);
    queue_transform_children(node, 0x00157D30, context.tree);
}
}
