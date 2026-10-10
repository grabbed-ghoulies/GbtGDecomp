#pragma once
#include "shader_validation_registers.h"
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
ShaderValidationChannelMask* shader_validation_unregistered_reader_channels(const ShaderValidationOwner& owner,
    ShaderValidationChannelMask& output, const ShaderValidationBalancedTree& registry,
    const ShaderValidationInstruction* instruction, const ShaderValidationSetContext& context); // Xbox 0x00190F87
bool move_shader_validation_masked_channels(ShaderValidationOwner& owner, ShaderValidationChannelMask mask,
    u8 climb, u8 propagate, const ShaderValidationSetContext& context); // Xbox 0x00191417
bool link_shader_validation_owners(ShaderValidationOwner* owner, ShaderValidationOwner* other, u8 query_only,
    ShaderValidationChannelMask owner_mask, ShaderValidationChannelMask other_mask,
    const ShaderValidationSetContext& context); // Xbox 0x00191DF3
bool reorder_shader_validation_owner_channels(ShaderValidationOwner& owner, ShaderValidationChannelOrder order,
    const ShaderValidationSetContext& context); // Xbox 0x001911A8
bool shader_validation_owner_dependencies_registered(const ShaderValidationOwner& owner, const ShaderValidationPass& pass,
    const ShaderValidationBalancedTree& writers, const ShaderValidationBalancedTree& readers,
    const ShaderValidationInstruction* instruction, ShaderValidationChannelMask mask,
    const ShaderValidationSetContext& context); // Xbox 0x00191D2D
bool shader_validation_instruction_accepts_order(const ShaderValidationInstruction& instruction, u32 slot,
    ShaderValidationChannelOrder order); // Xbox 0x00193C2A
bool shader_validation_operands_equivalent(const ShaderValidationOperand& operand, const ShaderValidationOperand& other,
    const ShaderValidationOwner* active_owner); // Xbox 0x00194311
bool retire_shader_validation_register(ShaderValidationPass& pass, i32 kind, const ShaderValidationBalancedTree& registry,
    const std::function<void(ShaderValidationPassNode&, u32)>& deleting_destructor,
    const std::function<void*(std::size_t, u32)>& allocate, const ShaderValidationSetContext& context); // Xbox 0x00193EE2
}
