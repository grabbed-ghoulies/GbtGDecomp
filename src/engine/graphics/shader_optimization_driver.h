#pragma once
#include "shader_validation_program.h"
#include "shader_merge_pass.h"
#include "shader_register_rewrite.h"

namespace gbtg
{
struct RuntimeShaderOptimizationContext
{
    ShaderValidationProgramContext& validation;
    u8* dead_write_frame; // Driver EBP-0x22C, 0x208 bytes.
    ShaderRegisterRewriteState& registers; // Pointer-width view of EBP-0x24.
    u8* reorder_frame; // Driver EBP-0xAF4.
    u8* multiply_frame; // 0x0018C1B9's own schedule frame.
    u8* first_cost_frame; // First 0x0018C277 call's schedule frame.
    u8* second_cost_frame; // Second 0x0018C277 call's schedule frame.
    RuntimeShaderMergeScratch& merge;
};
u32 optimize_runtime_vertex_shader(u8 pairs, u32 passes, u32 unused, u32 mode,
    u32* program, u32 scheduling, u8 validation, RuntimeShaderOptimizationContext& context); // Xbox 0x0018C563
u32 splice_runtime_vertex_shaders(u32* output, u32* size, u32* instruction_count,
    const u32* const* programs, u32 count, i32 optimize,
    RuntimeShaderOptimizationContext* context); // Xbox 0x0018D2F4
}
