#pragma once
#include "shader_validation_instruction_constructor.h"
#include "shader_validation_registers.h"
#include "shader_validation_sets.h"

namespace gbtg
{
struct ShaderValidationPassDestroyContext
{
    ShaderValidationEraseContext& expressions; // DAT_001a4d48 registry.
    ShaderValidationEraseContext& objects; // DAT_001a4d38 registry.
    std::function<void(ShaderValidationPassNode&, u32)> deleting_destructor; // Pass-node vtable slot +0x00.
    std::function<u32(const void*)> retail_address;
};

void destroy_shader_validation_pass_tree_object(ShaderValidationPassTreeObject& tree,
    const std::function<void(ShaderValidationPassNode&, u32)>& deleting_destructor); // Xbox 0x001914B9
ShaderValidationOwner* create_shader_validation_constant_owner(ShaderValidationConstructorContext& context,
    const ShaderValidationPass& pass, u32 source, u8 use_pass_owner); // Xbox 0x001935E6
void destroy_shader_validation_pass(ShaderValidationPassDestroyContext& context, ShaderValidationPass& pass); // Xbox 0x001934A5
}
