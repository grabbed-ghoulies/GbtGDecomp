#pragma once
#include "shader_validation_sets.h"

namespace gbtg
{
// Retail-address maps and allocation for validator sets built on the stack.
struct ShaderValidationSetContext
{
    std::function<void*(std::size_t, u32)> allocate;
    std::function<void(ShaderValidationBalancedNode&, u32)> deleting_destructor; // Set-node vtable slot +0x00.
    ShaderValidationInstructionAt instruction_at;
    ShaderValidationOperandAt operand_at;
    ShaderValidationAddressOf address_of;
};

bool shader_validation_owner_readers_registered(const ShaderValidationOwner& owner, const ShaderValidationBalancedTree& registry,
    ShaderValidationChannelMask mask, const ShaderValidationInstruction* instruction, u8 climb_derived, u8 climb_channel_owner,
    const ShaderValidationSetContext& context); // Xbox 0x00190E27
bool negate_shader_validation_owner_writers(ShaderValidationOwner& owner, const ShaderValidationSetContext& context); // Xbox 0x00192B84
}
