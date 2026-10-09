#include "shader_validation_channels.h"

namespace gbtg
{
// Xbox 0x0018E57C
void initialize_shader_validation_channel_order(ShaderValidationChannelOrder& order)
{
    order.vtable_address = 0x00203794;
    u32 index = 0;
    do
    {
        order.channels[index] = index;
        ++index;
    } while (index < 4);
}

// Xbox 0x0018E917
void copy_shader_validation_channel_mask(ShaderValidationChannelMask& destination, const ShaderValidationChannelMask& source)
{
    u8* output = destination.channels.data();
    const u8* input = source.channels.data();
    i32 remaining = 4;
    do
    {
        *output = *input;
        ++output;
        ++input;
        --remaining;
    } while (remaining != 0);
}
}
