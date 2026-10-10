#pragma once
#include "shader_validation_pass_lifecycle.h"

namespace gbtg
{
struct ShaderValidationProgramConstructorContext
{
    ShaderValidationDependencyContext& dependencies;
    ShaderValidationInsertContext& expressions; // DAT_001a4d48.
    std::function<void(ShaderValidationPassNode&, u32)> deleting_destructor;
    const ShaderValidationListVtable* list_node_vtable; // Retail 0x002037C4.
};

ShaderValidationInstruction* construct_shader_validation_packed_instruction(ShaderValidationProgramConstructorContext& context,
    ShaderValidationInstruction& instruction, u32 word0, u32 word1, u32 word2, u32 sequence,
    ShaderValidationPass& pass); // Xbox 0x00195FBE
ShaderValidationPass* construct_shader_validation_pass(ShaderValidationProgramConstructorContext& context,
    ShaderValidationPass& pass, const u32* packed_program); // Xbox 0x00196545
ShaderValidationProgramPass* construct_shader_validation_program_pass(ShaderValidationProgramConstructorContext& context,
    ShaderValidationProgramPass& pass, const u32* packed_program); // Xbox 0x00196739
}
