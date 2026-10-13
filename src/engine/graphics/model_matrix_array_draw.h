#pragma once
#include "model_tree_dispatch.h"

namespace gbtg
{
struct RuntimeModelMatrixScratch
{
    std::span<RuntimeShaderMatrix> matrices; // DAT_00437080.
    RuntimeShaderMatrix* current = nullptr; // DAT_00437090.
    u32 size = 0; // DAT_00437094, byte size.
};
u32 runtime_model_matrix_scratch_size(); // Xbox 0x00155D40
void bind_runtime_model_matrix_scratch(RuntimeShaderMatrix* matrices, u32 size,
    RuntimeModelMatrixScratch& scratch); // Xbox 0x00155D20
struct RuntimeModelMatrixArrayDraw
{
    ModelTreeDispatchContext& tree;
    RuntimeModelMatrixScratch& scratch;
    std::span<RuntimeShaderMatrix>& published; // Native render state +0x190 pointer.
};
void calculate_runtime_model_matrix_array(u32 node, const ModelPoseBuffer* pose,
    const RuntimeShaderMatrix& combined, const RuntimeShaderMatrix& camera,
    std::span<const u8> storage, RuntimeModelMatrixArrayDraw& context); // Xbox 0x00155E60
void dispatch_runtime_model_matrix_array(u32 node, RuntimeModelInstanceStorage& storage,
    RuntimeModelMatrixArrayDraw& context); // Xbox 0x00155F80
struct RuntimeModelRigidSkinDraw
{
    RuntimeModelMatrixArrayDraw& matrices;
    const RuntimeShaderMatrix& projection; // Render frame +0x84.
    const RuntimeShaderMatrixBoundaries& boundaries;
    const std::function<void(u32, const float*, u32)>& constants; // D3D constant upload, vector count.
};
void dispatch_runtime_model_rigid_skin(u32 node, void* storage,
    RuntimeModelRigidSkinDraw& context); // Xbox 0x00157B90
}
