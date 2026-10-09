#include "shader_validation_accessors.h"
#include <bit>

namespace gbtg
{
// Xbox 0x0018E414
ShaderValidationChannelMask* construct_shader_validation_channel_mask(ShaderValidationChannelMask& mask,
    u8 first, u8 second, u8 third, u8 fourth)
{
    mask.channels[0] = first;
    mask.channels[1] = second;
    mask.channels[2] = third;
    mask.vtable_address = 0x00203790;
    mask.channels[3] = fourth;
    return &mask;
}

// Xbox 0x0018E595
ShaderValidationChannelOrder* construct_shader_validation_channel_order(ShaderValidationChannelOrder& order,
    u32 first, u32 second, u32 third, u32 fourth)
{
    order.channels[0] = first;
    order.channels[1] = second;
    order.channels[2] = third;
    order.vtable_address = 0x00203794;
    order.channels[3] = fourth;
    return &order;
}

// Xbox 0x0018E5C3
bool shader_validation_channel_order_contains(const ShaderValidationChannelOrder& order, u32 channel)
{
    i32 index = 0;
    do
    {
        if (order.channels[index] == channel)
        {
            return true;
        }
        ++index;
    } while (index <= 3);
    return false;
}

// Xbox 0x0018E67C
u32 shader_validation_channel_order_keeps(const ShaderValidationChannelOrder& order, u32 index)
{
    // Callers pass a channel index from zero to four; four is the missing-channel value.
    if (index == 4)
    {
        return 1;
    }
    const u32 channel = order.channels[index];
    if (index == channel || channel == 4)
    {
        return 1;
    }
    return 0;
}

// Xbox 0x0018E6BE
u32 find_shader_validation_channel_order_index(const ShaderValidationChannelOrder& order, u32 channel)
{
    i32 index = 0;
    do
    {
        const u32 value = index == 4 ? 4u : order.channels[index];
        if (channel == value)
        {
            return static_cast<u32>(index);
        }
        ++index;
    } while (index <= 3);
    return 4;
}

namespace
{
ShaderValidationChannelMask* copy_shader_validation_mask_bytes(const ShaderValidationChannelMask& source,
    ShaderValidationChannelMask& result)
{
    result.vtable_address = 0x00203790;
    result.channels = source.channels; // One dword copy of the four channel bytes.
    return &result;
}
}

// Xbox 0x0018E833
ShaderValidationChannelMask* copy_shader_validation_output_mask(const ShaderValidationInstruction& instruction,
    ShaderValidationChannelMask& result)
{
    return copy_shader_validation_mask_bytes(instruction.output_mask, result);
}

// Xbox 0x0018E84E
ShaderValidationChannelMask* copy_shader_validation_vector_mask(const ShaderValidationInstruction& instruction,
    ShaderValidationChannelMask& result)
{
    return copy_shader_validation_mask_bytes(instruction.vector_mask, result);
}

// Xbox 0x0018E869
ShaderValidationChannelMask* copy_shader_validation_scalar_mask(const ShaderValidationInstruction& instruction,
    ShaderValidationChannelMask& result)
{
    return copy_shader_validation_mask_bytes(instruction.scalar_mask, result);
}

// Xbox 0x0018E884
u32 shader_validation_vector_writes_channel(const ShaderValidationInstruction& instruction, u32 index)
{
    if (instruction.vector_mask.channels[index] != 0)
    {
        return 1;
    }
    if (instruction.output_flag == 0 && instruction.output_mask.channels[index] != 0)
    {
        return 1;
    }
    return 0;
}

// Xbox 0x0018E8A5
u32 shader_validation_scalar_writes_channel(const ShaderValidationInstruction& instruction, u32 index)
{
    if (instruction.scalar_mask.channels[index] != 0)
    {
        return 1;
    }
    if (instruction.output_flag != 0 && instruction.output_mask.channels[index] != 0)
    {
        return 1;
    }
    return 0;
}

// Xbox 0x0018E8E4
u32 shader_validation_vector_opcode_is_add_or_mad(const ShaderValidationInstruction& instruction)
{
    const u32 opcode = instruction.vector_opcode;
    if (opcode == 3 || opcode == 4)
    {
        return 1;
    }
    return 0;
}

// Xbox 0x0018E8C6
u32 shader_validation_vector_opcode_is_add_through_dp4(const ShaderValidationInstruction& instruction)
{
    const i32 opcode = std::bit_cast<i32>(instruction.vector_opcode);
    if (opcode == 3 || opcode == 4 || (opcode >= 5 && opcode <= 7))
    {
        return 1;
    }
    return 0;
}

// Xbox 0x0018E8F8
u32 shader_validation_instruction_is_single_move(const ShaderValidationInstruction& instruction)
{
    if (instruction.vector_opcode == 1 && instruction.scalar_opcode == 0)
    {
        return 1;
    }
    if (instruction.scalar_opcode == 1 && instruction.vector_opcode == 0)
    {
        return 1;
    }
    return 0;
}

// Xbox 0x0018E76F
void set_shader_validation_owner_derived(ShaderValidationOwner& owner, ShaderValidationOwner* derived)
{
    if (derived != nullptr)
    {
        if (derived->channel_owner != nullptr)
        {
            derived->channel_owner->derived_owner = nullptr;
        }
        derived->channel_owner = &owner;
    }
    owner.derived_owner = derived;
}

// Xbox 0x0018F4F7
void set_shader_validation_owner_channel_owner(ShaderValidationOwner& owner, ShaderValidationOwner* channel_owner)
{
    if (channel_owner != nullptr)
    {
        if (channel_owner->derived_owner != nullptr)
        {
            channel_owner->derived_owner->channel_owner = nullptr;
        }
        channel_owner->derived_owner = &owner;
    }
    owner.channel_owner = channel_owner;
}

// Xbox 0x0018E7AF
u32 find_shader_validation_owner_channel(const ShaderValidationOwner* owner, u32 index)
{
    // The first owner is dereferenced without a null test, as in retail.
    do
    {
        if (owner->channels[index] != 0)
        {
            return owner->channels[index];
        }
        owner = owner->channel_owner;
    } while (owner != nullptr);
    return 0;
}

// Xbox 0x00190370
ShaderValidationPassNode* find_shader_validation_pass_node_signed(const ShaderValidationPassTree& tree, u32 key)
{
    // Retail pops a second, unused stack argument (RET 8).
    const i32 signed_key = std::bit_cast<i32>(key);
    ShaderValidationPassNode* node = tree.root;
    while (node != nullptr)
    {
        const i32 value = std::bit_cast<i32>(node->value);
        if (value > signed_key)
        {
            node = node->first;
        }
        else if (value < signed_key)
        {
            node = node->second;
        }
        else
        {
            break;
        }
    }
    return node;
}
}
