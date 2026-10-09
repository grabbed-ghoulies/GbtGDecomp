#pragma once
#include "shader_validation_channel_ops.h"
#include "shader_validation_erase.h"
#include <functional>

namespace gbtg
{
bool shader_validation_instruction_uses_slot(const ShaderValidationInstruction& instruction, u32 slot); // Xbox 0x0018E988
bool convert_shader_validation_vector_move(ShaderValidationInstruction& instruction, u8 check_writers); // Xbox 0x0018FFC9
bool shader_validation_instruction_reads_channel(const ShaderValidationInstruction& instruction, u32 slot,
    i32 channel); // Xbox 0x0018E9DC
bool shader_validation_operand_reads_channel(const ShaderValidationOperand& operand, i32 channel); // Xbox 0x0018F848
ShaderValidationChannelMask* shader_validation_instruction_read_mask(const ShaderValidationInstruction& instruction,
    ShaderValidationChannelMask& result, u32 slot); // Xbox 0x0018EB4C
bool shader_validation_instruction_reads_unset_channel(const ShaderValidationInstruction& instruction); // Xbox 0x0018EB96
bool shader_validation_operand_reads_order_channel(const ShaderValidationOperand& operand, u32 channel); // Xbox 0x001906BB
bool shader_validation_vector_result_reaches(const ShaderValidationInstruction& reader, u32 slot,
    const ShaderValidationInstruction& writer); // Xbox 0x0018FC2A
bool shader_validation_scalar_result_reaches(const ShaderValidationInstruction& reader, u32 slot,
    const ShaderValidationInstruction& writer); // Xbox 0x0018FD7D
// Channel arguments are 0..4 (4 is the missing channel); swaps write channels 0..3 only.
bool shader_validation_destination_channels_match(const ShaderValidationInstruction& instruction,
    const ShaderValidationOwner* destination, u32 first, u32 second); // Xbox 0x0018EBEA
bool swap_shader_validation_destination_channels(ShaderValidationInstruction& instruction,
    const ShaderValidationOwner* destination, u32 first, u32 second, u8 query_only); // Xbox 0x0018EDEE
bool shader_validation_operand_reads_masked_channel(const ShaderValidationOperand& operand,
    ShaderValidationChannelMask mask); // Xbox 0x001907E9
bool shader_validation_owner_channels_share_writer(const ShaderValidationOwner& owner, u32 first, u32 second); // Xbox 0x0018F815
ShaderValidationBalancedNode** find_shader_validation_tree_last(const ShaderValidationBalancedTree& tree,
    ShaderValidationBalancedNode** result); // Xbox 0x001900DE
bool shader_validation_owner_diagnostic_reads_channel(const ShaderValidationOwner& owner, u32 channel,
    const std::function<const ShaderValidationOperand*(u32)>& operand_at); // Xbox 0x00190D14
bool shader_validation_instruction_depends_on(const ShaderValidationInstruction& reader, const ShaderValidationInstruction& writer,
    const ShaderValidationOwner* active_owner); // Xbox 0x00190858
bool shader_validation_instructions_can_pair(const ShaderValidationInstruction& reader,
    const ShaderValidationInstruction& writer); // Xbox 0x0018FED0
bool shader_validation_instruction_must_follow(const ShaderValidationInstruction& reader, const ShaderValidationInstruction& writer,
    const ShaderValidationOwner* active_owner); // Xbox 0x001908AE
bool negate_shader_validation_move_source(ShaderValidationInstruction& instruction,
    const ShaderValidationOwner* destination); // Xbox 0x0018ED95
bool convert_shader_validation_scalar_move(ShaderValidationInstruction& instruction); // Xbox 0x0018FF4B
}
