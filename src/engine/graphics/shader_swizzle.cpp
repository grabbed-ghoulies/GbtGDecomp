#include "shader_swizzle.h"

namespace gbtg
{
namespace
{
// Retail operand mask table at 0x001A22D0.
constexpr u8 vector_fixed[48] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 14, 14, 0, 14, 15, 0, 15, 15, 0, 6, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0};
// Retail operand mask table at 0x001A2300.
constexpr u8 vector_masked[48] = {0, 0, 0, 15, 0, 0, 15, 15, 0, 15, 0, 15, 15, 15, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 15, 15, 0, 15, 15, 0, 15, 15, 0, 15, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
// Retail operand mask table at 0x001A2330.
constexpr u8 scalar_fixed[24] = {0, 0, 0, 0, 0, 0, 0, 0, 8, 0, 0, 8, 0, 0, 8, 0, 0, 8, 0, 0, 8, 0, 0, 13};
// Retail operand mask table at 0x001A2348.
constexpr u8 scalar_masked[24] = {0, 0, 0, 0, 0, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
}
// Xbox 0x00187584
void decode_runtime_shader_operand_masks(const u32* instruction, u8* masks)
{
    const u32 word = instruction[0];
    const u32 selected = (word >> 2) & 1;
    u8 vector_output;
    if (selected == 0)
    {
        vector_output = static_cast<u8>((word >> 12) & 15);
    }
    else
    {
        vector_output = 0;
    }
    u8 scalar_output;
    if (selected == 1)
    {
        scalar_output = static_cast<u8>((word >> 12) & 15);
    }
    else
    {
        scalar_output = 0;
    }
    const u8 vector_channels = static_cast<u8>(((word >> 24) & 15) | vector_output);
    const u8 scalar_channels = static_cast<u8>(((word >> 16) & 15) | scalar_output);
    for (u32 index = 0; index < 3; ++index)
    {
        const u32 vector_index = index + ((instruction[2] >> 25) & 15) * 3;
        const u32 scalar_index = index + (instruction[2] >> 29) * 3;
        masks[index] = (scalar_masked[scalar_index] & scalar_channels)
            | (vector_masked[vector_index] & vector_channels)
            | scalar_fixed[scalar_index] | vector_fixed[vector_index];
    }
}

// Xbox 0x00187621
void decode_runtime_shader_masked_swizzle(u8 mask, u8 w, u8 z, u8 y, u8 x, u8* output)
{
    if ((mask & 1) == 0)
    {
        x = 0xFF;
    }
    output[0] = x;
    if ((mask & 2) == 0)
    {
        y = 0xFF;
    }
    output[1] = y;
    if ((mask & 4) == 0)
    {
        z = 0xFF;
    }
    output[2] = z;
    if ((mask & 8) == 0)
    {
        w = 0xFF;
    }
    output[3] = w;
}

// Xbox 0x00187671
void decode_runtime_shader_swizzles(const u32* instruction, u8* output)
{
    u8 masks[3];
    decode_runtime_shader_operand_masks(instruction, masks);
    u32 word = instruction[2];
    decode_runtime_shader_masked_swizzle(masks[0], (word >> 10) & 3, (word >> 8) & 3,
        (word >> 6) & 3, (word >> 4) & 3, output);
    word = instruction[1];
    decode_runtime_shader_masked_swizzle(masks[1], (word >> 25) & 3, (word >> 23) & 3,
        (word >> 21) & 3, (word >> 19) & 3, output + 4);
    word = instruction[1];
    decode_runtime_shader_masked_swizzle(masks[2], (word >> 10) & 3, (word >> 8) & 3,
        (word >> 6) & 3, (word >> 4) & 3, output + 8);
}

// Xbox 0x001877CE
bool runtime_shader_swizzle_has_default_channels(const u8* channels)
{
    const u8 defaults[4] = {3, 2, 1, 0};
    for (u32 index = 0; index < 4; ++index)
    {
        if (channels[index] != 0xFF && channels[index] != defaults[index])
        {
            return false;
        }
    }
    return true;
}

// Xbox 0x0018780B
u8 runtime_shader_swizzle_fallback(const u8* channels)
{
    if (!runtime_shader_swizzle_has_default_channels(channels))
    {
        for (u32 index = 0; index < 4; ++index)
        {
            if (channels[index] != 0xFF)
            {
                return channels[index];
            }
        }
    }
    return 0xFF;
}

// Xbox 0x00187833
i32 resolve_runtime_shader_swizzle_channel(u8 channel, u8 fallback, u8 default_channel)
{
    if (channel == 0xFF)
    {
        channel = fallback;
        if (fallback == 0xFF)
        {
            channel = default_channel;
        }
    }
    return static_cast<signed char>(channel);
}

// Xbox 0x0018784D
void encode_runtime_shader_swizzles(u32* instruction, const u8* channels)
{
    u8 fallback = runtime_shader_swizzle_fallback(channels);
    instruction[2] ^= (static_cast<u32>(resolve_runtime_shader_swizzle_channel(channels[0], fallback, 3)) << 4 ^ instruction[2]) & 0x30;
    instruction[2] ^= (static_cast<u32>(resolve_runtime_shader_swizzle_channel(channels[1], fallback, 2)) << 6 ^ instruction[2]) & 0xC0;
    instruction[2] ^= (static_cast<u32>(resolve_runtime_shader_swizzle_channel(channels[2], fallback, 1)) << 8 ^ instruction[2]) & 0x300;
    instruction[2] ^= (static_cast<u32>(resolve_runtime_shader_swizzle_channel(channels[3], fallback, 0)) << 10 ^ instruction[2]) & 0xC00;
    fallback = runtime_shader_swizzle_fallback(channels + 4);
    instruction[1] ^= (static_cast<u32>(resolve_runtime_shader_swizzle_channel(channels[4], fallback, 3)) << 19 ^ instruction[1]) & 0x180000;
    instruction[1] ^= (static_cast<u32>(resolve_runtime_shader_swizzle_channel(channels[5], fallback, 2)) << 21 ^ instruction[1]) & 0x600000;
    instruction[1] ^= (static_cast<u32>(resolve_runtime_shader_swizzle_channel(channels[6], fallback, 1)) << 23 ^ instruction[1]) & 0x1800000;
    instruction[1] ^= (static_cast<u32>(resolve_runtime_shader_swizzle_channel(channels[7], fallback, 0)) << 25 ^ instruction[1]) & 0x6000000;
    fallback = runtime_shader_swizzle_fallback(channels + 8);
    instruction[1] ^= (static_cast<u32>(resolve_runtime_shader_swizzle_channel(channels[8], fallback, 3)) << 4 ^ instruction[1]) & 0x30;
    instruction[1] ^= (static_cast<u32>(resolve_runtime_shader_swizzle_channel(channels[9], fallback, 2)) << 6 ^ instruction[1]) & 0xC0;
    instruction[1] ^= (static_cast<u32>(resolve_runtime_shader_swizzle_channel(channels[10], fallback, 1)) << 8 ^ instruction[1]) & 0x300;
    instruction[1] ^= (static_cast<u32>(resolve_runtime_shader_swizzle_channel(channels[11], fallback, 0)) << 10 ^ instruction[1]) & 0xC00;
}

// Xbox 0x00187A01
bool merge_runtime_shader_swizzles(u32* output, const u32* first, const u32* second)
{
    u8 first_channels[12], second_channels[12], combined[12];
    decode_runtime_shader_swizzles(first, first_channels);
    decode_runtime_shader_swizzles(second, second_channels);
    for (u32 index = 0; index < 12; ++index)
    {
        u8 channel = first_channels[index];
        if (channel == 0xFF)
        {
            channel = second_channels[index];
        }
        else if (second_channels[index] != 0xFF && channel != second_channels[index])
        {
            return false;
        }
        combined[index] = channel;
    }
    encode_runtime_shader_swizzles(output, combined);
    return true;
}
}
