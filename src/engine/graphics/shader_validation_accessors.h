#pragma once
#include "shader_validation_pass_traversal.h"
#include "shader_validation_serializer.h"

namespace gbtg
{
ShaderValidationChannelMask* construct_shader_validation_channel_mask(ShaderValidationChannelMask& mask,
    u8 first, u8 second, u8 third, u8 fourth); // Xbox 0x0018E414
ShaderValidationChannelOrder* construct_shader_validation_channel_order(ShaderValidationChannelOrder& order,
    u32 first, u32 second, u32 third, u32 fourth); // Xbox 0x0018E595
bool shader_validation_channel_order_contains(const ShaderValidationChannelOrder& order, u32 channel); // Xbox 0x0018E5C3
u32 shader_validation_channel_order_keeps(const ShaderValidationChannelOrder& order, u32 index); // Xbox 0x0018E67C
u32 find_shader_validation_channel_order_index(const ShaderValidationChannelOrder& order, u32 channel); // Xbox 0x0018E6BE

ShaderValidationChannelMask* copy_shader_validation_output_mask(const ShaderValidationInstruction& instruction,
    ShaderValidationChannelMask& result); // Xbox 0x0018E833
ShaderValidationChannelMask* copy_shader_validation_vector_mask(const ShaderValidationInstruction& instruction,
    ShaderValidationChannelMask& result); // Xbox 0x0018E84E
ShaderValidationChannelMask* copy_shader_validation_scalar_mask(const ShaderValidationInstruction& instruction,
    ShaderValidationChannelMask& result); // Xbox 0x0018E869
u32 shader_validation_vector_writes_channel(const ShaderValidationInstruction& instruction, u32 index); // Xbox 0x0018E884
u32 shader_validation_scalar_writes_channel(const ShaderValidationInstruction& instruction, u32 index); // Xbox 0x0018E8A5
u32 shader_validation_vector_opcode_is_add_or_mad(const ShaderValidationInstruction& instruction); // Xbox 0x0018E8E4
u32 shader_validation_vector_opcode_is_add_through_dp4(const ShaderValidationInstruction& instruction); // Xbox 0x0018E8C6
u32 shader_validation_instruction_is_single_move(const ShaderValidationInstruction& instruction); // Xbox 0x0018E8F8

void set_shader_validation_owner_derived(ShaderValidationOwner& owner, ShaderValidationOwner* derived); // Xbox 0x0018E76F
void set_shader_validation_owner_channel_owner(ShaderValidationOwner& owner, ShaderValidationOwner* channel_owner); // Xbox 0x0018F4F7
ShaderValidationChannelWriter find_shader_validation_owner_channel(const ShaderValidationOwner* owner, u32 index); // Xbox 0x0018E7AF

ShaderValidationPassNode* find_shader_validation_pass_node_signed(const ShaderValidationPassTree& tree, u32 key); // Xbox 0x00190370
}
