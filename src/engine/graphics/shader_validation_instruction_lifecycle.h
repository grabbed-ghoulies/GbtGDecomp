#pragma once
#include "shader_validation_dependencies.h"

namespace gbtg
{
struct ShaderValidationInstructionDestroyContext
{
    ShaderValidationDependencyContext& dependencies;
    std::function<ShaderValidationBalancedChildTree*(u32)> set_at;
};
struct ShaderValidationOwnerDestroyContext
{
    ShaderValidationInstructionDestroyContext& instructions;
    ShaderValidationBalancedTree& expressions; // DAT_001a4d48.
    // Genuine expression-object vtable slot +0x00: 0x001936CD for registered pass trees,
    // 0x001928C6 for owner worklists. Maps the registry's retail object address.
    std::function<void(u32, ShaderValidationOwner&)> unregister_owner;
};

void destroy_shader_validation_operand(ShaderValidationConstructorContext& context,
    ShaderValidationOperand& operand); // Xbox 0x00193BBC
ShaderValidationOperand* delete_shader_validation_operand(ShaderValidationConstructorContext& context,
    ShaderValidationOperand* operand, u8 flags); // Xbox 0x00193FC0
void destroy_shader_validation_instruction(ShaderValidationInstructionDestroyContext& context,
    ShaderValidationInstruction& instruction); // Xbox 0x00192D13
ShaderValidationInstruction* delete_shader_validation_instruction(ShaderValidationInstructionDestroyContext& context,
    ShaderValidationInstruction* instruction, u8 flags); // Xbox 0x001937D8
bool merge_shader_validation_instructions(ShaderValidationInstructionDestroyContext& context,
    ShaderValidationInstruction& instruction, ShaderValidationInstruction& other); // Xbox 0x001956D1
void destroy_shader_validation_owner(ShaderValidationOwnerDestroyContext& context, ShaderValidationOwner& owner); // Xbox 0x001937FC
ShaderValidationOwner* delete_shader_validation_owner(ShaderValidationOwnerDestroyContext& context,
    ShaderValidationOwner* owner, u8 flags); // Xbox 0x00194088
}
