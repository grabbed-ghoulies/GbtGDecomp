#include "model_matrix_array_draw.h"
#include "d3dx_matrix.h"
#include <cstring>

namespace gbtg
{
namespace
{
RuntimeShaderMatrix matrix_at(const RuntimeRelocationImage& image, u32 address)
{
    RuntimeShaderMatrix matrix;
    std::memcpy(matrix.data(), image.pointer(address), 0x40);
    return matrix;
}

RuntimeShaderMatrix storage_matrix(std::span<const u8> storage, u32 offset)
{
    RuntimeShaderMatrix matrix;
    std::memcpy(matrix.data(), storage.data() + offset, 0x40);
    return matrix;
}
}

// Xbox 0x00155D40
u32 runtime_model_matrix_scratch_size()
{
    return 0x1F400;
}

// Xbox 0x00155D20
void bind_runtime_model_matrix_scratch(RuntimeShaderMatrix* matrices, u32 size,
    RuntimeModelMatrixScratch& scratch)
{
    scratch.matrices = {matrices, size / 0x40};
    scratch.size = size;
    scratch.current = matrices;
}

// Xbox 0x00155E60
void calculate_runtime_model_matrix_array(u32 node, const ModelPoseBuffer* pose,
    const RuntimeShaderMatrix& combined, const RuntimeShaderMatrix& camera,
    std::span<const u8> storage, RuntimeModelMatrixArrayDraw& context)
{
    const auto& image = context.tree.image;
    i32 index = 0;
    if (i32(image.word(node, 0x24)) > 0)
    {
        do
        {
            const u32 row = image.word(node, 0x20) + u32(index) * 8;
            const u8 flags = image.byte(row, 5);
            const u8 dynamic_index = image.byte(row, 6);
            auto& output = context.scratch.matrices[index];
            if ((flags & 1) != 0)
            {
                const u32 base = image.word(node, 0xC) + 4;
                const u32 flag_offset = base + image.word(node, 0x30) * 0x40 + dynamic_index;
                if ((storage[flag_offset] & 1) != 0)
                {
                    const auto matrix = storage_matrix(storage, base + u32(dynamic_index) * 0x40);
                    multiply_runtime_graphics_matrices(output.data(), matrix.data(), camera.data());
                }
                else
                {
                    const auto matrix = matrix_at(image, image.word(node, 0x2C) + u32(dynamic_index) * 0x40);
                    multiply_runtime_matrix(output, matrix, combined);
                }
            }
            else if ((flags & 2) != 0)
            {
                multiply_runtime_matrix(output, pose->packed_matrices[image.half(row)], combined);
            }
            else
            {
                const u32 base = image.word(node, 0xC) + 4;
                const u32 flag_offset = base + image.word(node, 0x30) * 0x40 + dynamic_index;
                if ((storage[flag_offset] & 1) != 0)
                {
                    const auto matrix = storage_matrix(storage, base + u32(dynamic_index) * 0x40);
                    multiply_runtime_graphics_matrices(output.data(), matrix.data(), camera.data());
                }
                else
                {
                    multiply_runtime_matrix(output, pose->packed_matrices[image.half(row)], combined);
                    const u8 static_index = image.byte(image.word(node, 0x20) + u32(index) * 8, 6);
                    const auto matrix = matrix_at(image, image.word(node, 0x2C) + u32(static_index) * 0x40);
                    multiply_runtime_matrix(output, matrix, output);
                }
            }
            ++index;
        }
        while (index < i32(image.word(node, 0x24)));
    }
    runtime_render_state(context.tree.renderer);
    context.published = context.scratch.matrices;
}

// Xbox 0x00155F80
void dispatch_runtime_model_matrix_array(u32 node, RuntimeModelInstanceStorage& storage,
    RuntimeModelMatrixArrayDraw& context)
{
    const auto& frame = runtime_render_frame(context.tree.renderer);
    const auto& state = runtime_render_state(context.tree.renderer);
    const size_t index = size_t(&frame - context.tree.renderer.frames.data());
    calculate_runtime_model_matrix_array(node, state.skeleton_pose, context.tree.combined[index],
        *context.tree.camera[index], storage.bytes, context);
    queue_runtime_model_group(node, context.tree);
}

// Xbox 0x00157B90
void dispatch_runtime_model_rigid_skin(u32 node, void* storage,
    RuntimeModelRigidSkinDraw& context)
{
    auto& tree = context.matrices.tree;
    runtime_render_state(tree.renderer);
    runtime_render_frame(tree.renderer);
    context.constants(0x74, context.projection.data(), 4);
    u32 index = 0;
    if (tree.image.word(node, 0x24) != 0)
    {
        do
        {
            const u8 matrix_index = tree.image.byte(tree.image.word(node, 0x20), index);
            RuntimeShaderMatrix transpose;
            context.boundaries.transpose(transpose, context.matrices.published[matrix_index]);
            context.constants(0x78 + index * 3, transpose.data(), 3);
            ++index;
        }
        while (index < tree.image.word(node, 0x24));
    }
    queue_runtime_model_group(node, tree);
}
}
