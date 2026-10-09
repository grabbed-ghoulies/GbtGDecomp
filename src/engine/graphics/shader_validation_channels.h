#pragma once
#include "shader_validation_storage.h"

namespace gbtg
{
struct ShaderValidationChannelOrder
{
    u32 vtable_address; // Retail +0x00.
    std::array<u32, 4> channels; // Retail +0x04.
};

void initialize_shader_validation_channel_order(ShaderValidationChannelOrder& order); // Xbox 0x0018E57C
void copy_shader_validation_channel_mask(ShaderValidationChannelMask& destination, const ShaderValidationChannelMask& source); // Xbox 0x0018E917
}
