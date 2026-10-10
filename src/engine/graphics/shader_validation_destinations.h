#pragma once
#include "shader_validation_serializer.h"

namespace gbtg
{
void set_shader_validation_channel_writer(ShaderValidationOwner& owner, u32 channel,
    ShaderValidationChannelWriter writer); // Xbox 0x00190D50
void remove_shader_validation_destination_channel(ShaderValidationInstruction& instruction, u32 channel,
    ShaderValidationOwner* owner); // Xbox 0x001908FC
ShaderValidationOwner* replace_shader_validation_output_destination(ShaderValidationInstruction& instruction,
    ShaderValidationOwner* owner, ShaderValidationChannelMask mask); // Xbox 0x0018F608
ShaderValidationOwner* replace_shader_validation_vector_destination(ShaderValidationInstruction& instruction,
    ShaderValidationOwner* owner, ShaderValidationChannelMask mask); // Xbox 0x0018F6B7
ShaderValidationOwner* replace_shader_validation_scalar_destination(ShaderValidationInstruction& instruction,
    ShaderValidationOwner* owner, ShaderValidationChannelMask mask); // Xbox 0x0018F766
void set_shader_validation_output_destination(ShaderValidationInstruction& instruction, ShaderValidationOwner* owner); // Xbox 0x0019070B
void set_shader_validation_vector_destination(ShaderValidationInstruction& instruction, ShaderValidationOwner* owner); // Xbox 0x0019072E
void set_shader_validation_scalar_destination(ShaderValidationInstruction& instruction, ShaderValidationOwner* owner); // Xbox 0x00190751
void set_shader_validation_masked_writers(ShaderValidationOwner& owner, ShaderValidationChannelMask mask,
    ShaderValidationChannelWriter writer); // Xbox 0x00191C0B
bool merge_shader_validation_duplicate_channel(ShaderValidationOwner& owner, u32 first, u32 second,
    const std::function<ShaderValidationOperand*(u32)>& operand_at); // Xbox 0x00190D77
void merge_shader_validation_duplicate_channels(ShaderValidationOwner& owner,
    const std::function<ShaderValidationOperand*(u32)>& operand_at); // Xbox 0x00191C47
}
