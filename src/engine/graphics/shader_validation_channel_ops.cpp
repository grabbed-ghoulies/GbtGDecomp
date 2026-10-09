#include "shader_validation_channel_ops.h"

namespace gbtg
{
// Xbox 0x0018E6DF
bool shader_validation_channel_order_covers_all(const ShaderValidationChannelOrder& order)
{
    i32 channel = 0;
    do
    {
        if (!shader_validation_channel_order_contains(order, static_cast<u32>(channel)))
        {
            return false;
        }
        ++channel;
    } while (channel <= 3);
    return true;
}

// Xbox 0x0018F394
bool shader_validation_channel_order_keeps_all(const ShaderValidationChannelOrder& order)
{
    i32 index = 0;
    do
    {
        if ((shader_validation_channel_order_keeps(order, static_cast<u32>(index)) & 0xFFu) == 0)
        {
            return false;
        }
        ++index;
    } while (index <= 3);
    return true;
}

// Xbox 0x0018E5E2
u32 find_shader_validation_uncovered_channel(const ShaderValidationChannelOrder& order)
{
    i32 channel = 0;
    do
    {
        if (!shader_validation_channel_order_contains(order, static_cast<u32>(channel)))
        {
            return static_cast<u32>(channel);
        }
        ++channel;
    } while (channel <= 3);
    return 4;
}

// Xbox 0x0018E69C
u32 find_shader_validation_moved_channel(const ShaderValidationChannelOrder& order)
{
    i32 index = 0;
    do
    {
        if ((shader_validation_channel_order_keeps(order, static_cast<u32>(index)) & 0xFFu) == 0)
        {
            return static_cast<u32>(index);
        }
        ++index;
    } while (index <= 3);
    return 4;
}

// Xbox 0x0018E741
bool shader_validation_channel_orders_compatible(const ShaderValidationChannelOrder& order,
    ShaderValidationChannelOrder other)
{
    i32 index = 0;
    do
    {
        if (index != 4)
        {
            const u32 channel = order.channels[index];
            if (channel != 4)
            {
                const u32 other_channel = other.channels[index];
                if (other_channel != 4 && channel != other_channel)
                {
                    return false;
                }
            }
        }
        ++index;
    } while (index <= 3);
    return true;
}

// Xbox 0x0018E548
ShaderValidationChannelMask* invert_shader_validation_channel_mask(const ShaderValidationChannelMask& mask,
    ShaderValidationChannelMask& result)
{
    construct_shader_validation_channel_mask(result, mask.channels[0] == 0 ? 1 : 0, mask.channels[1] == 0 ? 1 : 0,
        mask.channels[2] == 0 ? 1 : 0, mask.channels[3] == 0 ? 1 : 0);
    return &result;
}

// Xbox 0x0018E604
bool shader_validation_channel_repeats_before(const ShaderValidationChannelOrder& order, i32 position)
{
    i32 index = 0;
    if (position > 0)
    {
        do
        {
            if (index != 4)
            {
                const u32 channel = order.channels[index];
                if (channel != 4)
                {
                    const u32 other = position == 4 ? 4u : order.channels[position];
                    if (channel == other)
                    {
                        return true;
                    }
                }
            }
            ++index;
        } while (index < position);
    }
    return false;
}

// Xbox 0x0018E63F
u32 find_shader_validation_repeated_channel(const ShaderValidationChannelOrder& order)
{
    i32 position = 0;
    do
    {
        if (shader_validation_channel_repeats_before(order, position))
        {
            return static_cast<u32>(position);
        }
        ++position;
    } while (position <= 3);
    return 4;
}

// Xbox 0x0018E661
void clear_shader_validation_repeated_channels(ShaderValidationChannelOrder& order)
{
    for (u32 position = find_shader_validation_repeated_channel(order); position != 4;
         position = find_shader_validation_repeated_channel(order))
    {
        order.channels[position] = 4;
    }
}

// Xbox 0x0018E700
ShaderValidationChannelOrder* invert_shader_validation_channel_order(const ShaderValidationChannelOrder& order,
    ShaderValidationChannelOrder& result)
{
    const u32 fourth = find_shader_validation_channel_order_index(order, 3);
    const u32 third = find_shader_validation_channel_order_index(order, 2);
    const u32 second = find_shader_validation_channel_order_index(order, 1);
    const u32 first = find_shader_validation_channel_order_index(order, 0);
    construct_shader_validation_channel_order(result, first, second, third, fourth);
    return &result;
}

// Xbox 0x0018F355
ShaderValidationChannelMask* shader_validation_channel_order_used_mask(const ShaderValidationChannelOrder& order,
    ShaderValidationChannelMask& result)
{
    // The local mask's vtable slot is never written or read; only its channel bytes are returned.
    ShaderValidationChannelMask used;
    decode_shader_validation_channel_mask(used, 0xF);
    i32 channel = 0;
    do
    {
        used.channels[channel] = shader_validation_channel_order_contains(order, static_cast<u32>(channel)) ? 1 : 0;
        ++channel;
    } while (channel <= 3);
    result.vtable_address = 0x00203790;
    result.channels = used.channels;
    return &result;
}

// Xbox 0x0018F595
ShaderValidationChannelMask* shader_validation_owner_channel_mask(const ShaderValidationOwner& owner,
    ShaderValidationChannelMask& result)
{
    ShaderValidationChannelMask present;
    decode_shader_validation_channel_mask(present, 0xF);
    i32 channel = 0;
    do
    {
        present.channels[channel] = owner.channels[channel] != 0 ? 1 : 0;
        ++channel;
    } while (channel <= 3);
    result.vtable_address = 0x00203790;
    result.channels = present.channels;
    return &result;
}

// Xbox 0x0018F86B
ShaderValidationChannelMask* shader_validation_operand_source_mask(const ShaderValidationOperand& operand,
    ShaderValidationChannelMask mask, ShaderValidationChannelMask& result)
{
    // Order entries are 0..4 at every retail call site; larger values would index past the local mask.
    ShaderValidationChannelMask sources;
    decode_shader_validation_channel_mask(sources, 0);
    i32 index = 0;
    do
    {
        if (index != 4)
        {
            const u32 channel = operand.channel_order.channels[index];
            if (channel != 4 && mask.channels[index] != 0)
            {
                sources.channels[channel] = 1;
            }
        }
        ++index;
    } while (index <= 3);
    result.vtable_address = 0x00203790;
    result.channels = sources.channels;
    return &result;
}

// Xbox 0x0018F436
ShaderValidationChannelOrder* compose_shader_validation_channel_orders(const ShaderValidationChannelOrder& order,
    ShaderValidationChannelOrder other, ShaderValidationChannelOrder& result)
{
    // Order entries are 0..4 at every retail call site; four selects a missing channel.
    ShaderValidationChannelOrder composed;
    initialize_shader_validation_channel_order(composed);
    i32 index = 0;
    do
    {
        u32 channel = 4;
        if (index != 4)
        {
            const u32 source = order.channels[index];
            if (source != 4)
            {
                channel = other.channels[source];
            }
        }
        composed.channels[index] = channel;
        ++index;
    } while (index <= 3);
    result.channels = composed.channels;
    result.vtable_address = 0x00203794;
    return &result;
}

// Xbox 0x0018E4EB
ShaderValidationChannelMask* intersect_shader_validation_channel_masks(const ShaderValidationChannelMask& mask,
    ShaderValidationChannelMask other, ShaderValidationChannelMask& result)
{
    const u8 fourth = mask.channels[3] != 0 && other.channels[3] != 0 ? 1 : 0;
    const u8 third = mask.channels[2] != 0 && other.channels[2] != 0 ? 1 : 0;
    const u8 second = mask.channels[1] != 0 && other.channels[1] != 0 ? 1 : 0;
    const u8 first = mask.channels[0] != 0 && other.channels[0] != 0 ? 1 : 0;
    construct_shader_validation_channel_mask(result, first, second, third, fourth);
    return &result;
}

// Xbox 0x0018E48C
ShaderValidationChannelMask* unite_shader_validation_channel_masks(const ShaderValidationChannelMask& mask,
    ShaderValidationChannelMask other, ShaderValidationChannelMask& result)
{
    const u8 fourth = mask.channels[3] != 0 || other.channels[3] != 0 ? 1 : 0;
    const u8 third = mask.channels[2] != 0 || other.channels[2] != 0 ? 1 : 0;
    const u8 second = mask.channels[1] != 0 || other.channels[1] != 0 ? 1 : 0;
    const u8 first = mask.channels[0] != 0 || other.channels[0] != 0 ? 1 : 0;
    construct_shader_validation_channel_mask(result, first, second, third, fourth);
    return &result;
}
}
