#pragma once
#include "shader_validation_instruction_lifecycle.h"
#include "shader_validation_pass_lifecycle.h"

namespace gbtg
{
struct ShaderValidationOptimizationContext
{
    ShaderValidationOwnerDestroyContext& owners;
    ShaderValidationInsertContext& expressions;
    ShaderValidationMoveFrame& move_frame;
    std::function<ShaderValidationOwner*(u32)> owner_at;
};
void normalize_shader_validation_instruction(ShaderValidationOptimizationContext& context,
    ShaderValidationProgramPass& pass, ShaderValidationInstruction* instruction,
    ShaderValidationBalancedChildTree& owners, ShaderValidationBalancedChildTree& instructions); // Xbox 0x001943CD
void normalize_shader_validation_instruction_worklist(ShaderValidationOptimizationContext& context,
    ShaderValidationProgramPass& pass, ShaderValidationBalancedChildTree& owners,
    ShaderValidationBalancedChildTree& instructions); // Xbox 0x00195B89
void clean_shader_validation_owner_worklist(ShaderValidationOptimizationContext& context,
    ShaderValidationProgramPass& pass, ShaderValidationBalancedChildTree& owners,
    ShaderValidationBalancedChildTree& instructions); // Xbox 0x00195E43
void optimize_shader_validation_program_worklists(ShaderValidationOptimizationContext& context,
    ShaderValidationProgramPass& pass); // Xbox 0x00196B22
void optimize_shader_validation_program(ShaderValidationOptimizationContext& context,
    ShaderValidationProgramPass& pass, u8 split_scalar_instructions); // Xbox 0x00197223
}
