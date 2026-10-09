#include "shader_validation_operand_order.h"
#include <cstring>

namespace gbtg
{
// Xbox 0x0018E442
u32 pack_shader_validation_channel_mask(const ShaderValidationChannelMask& mask)
{
    return (mask.channels[0] != 0 ? 8u : 0u) +
           (mask.channels[1] != 0 ? 4u : 0u) +
           (mask.channels[2] != 0 ? 2u : 0u) +
           (mask.channels[3] != 0 ? 1u : 0u);
}

// Xbox 0x0018E7D3
ShaderValidationChannelOrder* copy_shader_validation_operand_order(ShaderValidationChannelOrder& output, const u8* operand)
{
    output.vtable_address = 0x00203794;
    // MOVSD reads and writes each word in sequence, including overlapping views.
    for (u32 index = 0; index < 4; ++index)
    {
        u32 channel;
        std::memcpy(&channel, operand + 0x1c + index * 4, sizeof(channel));
        output.channels[index] = channel;
    }
    return &output;
}

// Xbox 0x0018F3B5
ShaderValidationChannelOrder* fill_shader_validation_missing_channels(ShaderValidationChannelOrder& output, const ShaderValidationChannelOrder& input)
{
    ShaderValidationChannelOrder temporary;
    initialize_shader_validation_channel_order(temporary);
    i32 destination = 0;
    do
    {
        u32 channel = input.channels[destination];
        if (channel == 4)
        {
            i32 source = destination;
            while (source == 4 || (input.channels[source] == 4 && source > 0))
            {
                --source;
            }
            if (input.channels[source] == 4)
            {
                while (source != 4)
                {
                    if (input.channels[source] != 4 || source >= 3)
                    {
                        break;
                    }
                    ++source;
                }
            }
            channel = source == 4 ? 0 : input.channels[source];
            if (channel == 4)
            {
                channel = 0;
            }
        }
        temporary.channels[destination] = channel;
        ++destination;
    } while (destination <= 3);
    output.channels[0] = temporary.channels[0];
    output.channels[1] = temporary.channels[1];
    output.channels[2] = temporary.channels[2];
    output.channels[3] = temporary.channels[3];
    output.vtable_address = 0x00203794;
    return &output;
}
}
