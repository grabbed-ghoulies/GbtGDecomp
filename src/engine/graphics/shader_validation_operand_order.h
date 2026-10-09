#pragma once
#include "shader_validation_channels.h"

namespace gbtg
{
u32 pack_shader_validation_channel_mask(const ShaderValidationChannelMask& mask); // Xbox 0x0018E442
ShaderValidationChannelOrder* copy_shader_validation_operand_order(ShaderValidationChannelOrder& output, const u8* operand); // Xbox 0x0018E7D3
ShaderValidationChannelOrder* fill_shader_validation_missing_channels(ShaderValidationChannelOrder& output, const ShaderValidationChannelOrder& input); // Xbox 0x0018F3B5
}
