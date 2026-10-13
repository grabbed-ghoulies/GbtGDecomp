#pragma once
#include "model_tree_dispatch.h"

namespace gbtg
{
struct RuntimeModelTransformDraw
{
    ModelTreeDispatchContext& tree;
    std::span<const RuntimeShaderMatrix*> cameras; // Writable frame +0x40 slots.
    std::function<RuntimeShaderMatrix&(i32)> projection; // Frame +0x84.
    u16& projection_register; // Render state +0x194.
    u16& world_register; // Render state +0x196.
    const RuntimeShaderMatrixBoundaries& matrices;
    std::function<void(u32, const float*, u32)> constants; // Vector count.
};
void push_runtime_model_frame(RuntimeModelTransformDraw& context); // Xbox 0x00156E00
void pop_runtime_model_frame(RuntimeModelRenderContext& renderer); // Xbox 0x00156E30
void translate_runtime_model_joint_matrix(u32 joint, const RuntimeShaderMatrix& input,
    RuntimeShaderMatrix& output, const RuntimeRelocationImage& image); // Xbox 0x00156A30
void upload_runtime_model_frame_matrix(const RuntimeShaderMatrix& matrix,
    RuntimeModelTransformDraw& context); // Xbox 0x00156540
void apply_runtime_model_node_transform(u32 node, std::span<const u8> storage,
    RuntimeModelTransformDraw& context); // Xbox 0x00157F10
void restore_runtime_model_transform(RuntimeModelTransformDraw& context); // Xbox 0x00157ED0
void restore_runtime_model_billboard(RuntimeModelTransformDraw& context); // Xbox 0x00157D30
void dispatch_runtime_model_transform(u32 node, std::span<const u8> storage,
    RuntimeModelTransformDraw& context); // Xbox 0x00157FC0
void dispatch_runtime_model_billboard(u32 node, std::span<const u8> storage,
    RuntimeModelTransformDraw& context); // Xbox 0x00157D50
}
