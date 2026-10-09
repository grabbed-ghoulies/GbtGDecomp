#include "shader_validation_serializer.h"
#include "shader_validation_reserved_register.h"

namespace gbtg
{
// Xbox 0x0018E7F6
ShaderValidationChannelOrder* assign_shader_validation_channel_order(ShaderValidationChannelOrder& destination,
    const ShaderValidationChannelOrder& source)
{
    // Copies the four words after the vtable; the destination vtable is retained.
    for (u32 index = 0; index < 4; ++index)
    {
        destination.channels[index] = source.channels[index];
    }
    return &destination;
}

// Xbox 0x0018F5D7
ShaderValidationChannelOrder* replace_shader_validation_operand_channel_order(ShaderValidationChannelOrder& result,
    ShaderValidationOperand& operand, ShaderValidationChannelOrder value)
{
    const ShaderValidationChannelOrder* assigned = assign_shader_validation_channel_order(operand.channel_order, value);
    result.vtable_address = 0x00203794;
    result.channels[0] = assigned->channels[0];
    result.channels[1] = assigned->channels[1];
    result.channels[2] = assigned->channels[2];
    result.channels[3] = assigned->channels[3];
    return &result;
}

// Xbox 0x0018E7D3
ShaderValidationChannelOrder* copy_shader_validation_operand_order(ShaderValidationChannelOrder& output,
    const ShaderValidationOperand& operand)
{
    // Typed overload of the raw +0x1C reader: the operand's channel words follow its order vtable.
    output.vtable_address = 0x00203794;
    output.channels[0] = operand.channel_order.channels[0];
    output.channels[1] = operand.channel_order.channels[1];
    output.channels[2] = operand.channel_order.channels[2];
    output.channels[3] = operand.channel_order.channels[3];
    return &output;
}

// Xbox 0x0018E973
ShaderValidationOwner* find_shader_validation_foreign_parent(const ShaderValidationOwner& owner,
    const ShaderValidationOwner* active_owner)
{
    if (owner.operand == nullptr)
    {
        return nullptr;
    }
    ShaderValidationOwner* parent = owner.operand->parent;
    if (parent == active_owner)
    {
        return nullptr;
    }
    return parent;
}

// Xbox 0x0018E816
u32 select_shader_validation_destination_index(const ShaderValidationInstruction& instruction)
{
    const ShaderValidationOwner* destination = instruction.vector_opcode != 0
        ? instruction.vector_destination
        : instruction.scalar_destination;
    if (destination == nullptr)
    {
        return 0xF;
    }
    return destination->source;
}

namespace
{
u32 pack_shader_validation_operand_swizzle(const ShaderValidationOperand& operand)
{
    u32 value = (static_cast<u32>(operand.marker) & 1u) << 2 | (operand.channel_order.channels[0] & 3u);
    value = value << 2 | (operand.channel_order.channels[1] & 3u);
    value = value << 2 | (operand.channel_order.channels[2] & 3u);
    value = value << 2 | (operand.channel_order.channels[3] & 3u);
    return value;
}

u32 shader_validation_parent_source(const ShaderValidationOwner* parent)
{
    return parent != nullptr ? parent->source : 0;
}

u32 shader_validation_parent_kind(const ShaderValidationOwner* parent)
{
    if (parent == nullptr || parent->kind == 0)
    {
        return 2;
    }
    return parent->kind;
}
}

// Xbox 0x0018F8B7
u32* serialize_shader_validation_instruction(ShaderValidationInstruction& instruction, u32* output,
    ShaderValidationSerializerFrame& frame, const ShaderValidationOwner* active_owner)
{
    u32 packed = ((instruction.scalar_opcode << 4 | (instruction.vector_opcode & 0xFu)) << 0x19) |
                 (frame.word2 & 0x1FFFFu);
    packed &= 0xFE01FFFFu;
    frame.word0 &= 0xFFFFFFFDu;
    for (u32 index = 0; index < 3; ++index)
    {
        ShaderValidationOwner* parent = instruction.operands[index]->parent;
        if (parent != nullptr && parent->kind == 3)
        {
            packed ^= ((parent->source << 0x11) ^ packed) & 0x1FE0000u;
            const u32 foreign = find_shader_validation_foreign_parent(*parent, active_owner) != nullptr ? 1u : 0u;
            frame.word0 ^= ((foreign << 1) ^ frame.word0) & 2u;
        }
    }
    packed &= 0xFFFE1FFFu;
    for (u32 index = 0; index < 3; ++index)
    {
        const ShaderValidationOwner* parent = instruction.operands[index]->parent;
        if (parent != nullptr && parent->kind == 2)
        {
            packed ^= ((parent->source << 0xD) ^ packed) & 0x1E000u;
        }
    }
    for (u32 index = 0; index < 3; ++index)
    {
        ShaderValidationOperand& operand = *instruction.operands[index];
        ShaderValidationChannelOrder copied;
        const ShaderValidationChannelOrder* order = copy_shader_validation_operand_order(copied, operand);
        ShaderValidationChannelOrder argument; // By-value stack argument written in full by 0x0018F3B5.
        fill_shader_validation_missing_channels(argument, *order);
        ShaderValidationChannelOrder result;
        replace_shader_validation_operand_channel_order(result, operand, argument);
    }

    const ShaderValidationOperand& first = *instruction.operands[0];
    packed ^= (shader_validation_parent_source(first.parent) ^ packed) & 0xFu;
    frame.word1 ^= ((shader_validation_parent_kind(first.parent) << 0x1C) ^ frame.word1) & 0x30000000u;
    if ((frame.word1 & 0x30000000u) == 0)
    {
        frame.word1 = (frame.word1 & 0xDFFFFFFFu) | 0x10000000u;
        packed = (packed & 0xFFFFFFFCu) | 0xCu;
    }
    frame.word2 = pack_shader_validation_operand_swizzle(first) << 4 | (packed & 0xFFFFE00Fu);

    const ShaderValidationOperand& second = *instruction.operands[1];
    frame.word1 ^= ((shader_validation_parent_source(second.parent) << 0xF) ^ frame.word1) & 0x78000u;
    frame.word1 ^= ((shader_validation_parent_kind(second.parent) << 0xD) ^ frame.word1) & 0x6000u;
    if ((frame.word1 & 0x6000u) == 0)
    {
        frame.word1 = (frame.word1 & 0xFFFE3FFFu) | 0x62000u;
    }
    u32 middle = pack_shader_validation_operand_swizzle(second) << 0x13 | (frame.word1 & 0xF007FFFFu);

    const ShaderValidationOperand& third = *instruction.operands[2];
    middle ^= (shader_validation_parent_source(third.parent) ^ middle) & 0xFu;
    frame.word1 = middle;
    frame.word0 ^= ((shader_validation_parent_kind(third.parent) << 0x1C) ^ frame.word0) & 0x30000000u;
    if ((frame.word0 & 0x30000000u) == 0)
    {
        frame.word0 = (frame.word0 & 0xDFFFFFFFu) | 0x10000000u;
        frame.word1 = (middle & 0xFFFFFFFCu) | 0xCu;
    }
    frame.word1 = pack_shader_validation_operand_swizzle(third) << 4 | (frame.word1 & 0xFFFFE00Fu);

    u32 head = frame.word0;
    head ^= ((pack_shader_validation_channel_mask(instruction.output_mask) << 0xC) ^ head) & 0xF000u;
    u32 vector_bits;
    if (instruction.vector_opcode == 0xD)
    {
        ShaderValidationChannelMask empty;
        empty.vtable_address = 0x00203790;
        decode_shader_validation_channel_mask(empty, 0);
        vector_bits = pack_shader_validation_channel_mask(empty);
    }
    else
    {
        vector_bits = pack_shader_validation_channel_mask(instruction.vector_mask);
    }
    const u32 scalar_bits = pack_shader_validation_channel_mask(instruction.scalar_mask);
    u32 masks = ((vector_bits & 0xFu) << 8 | (scalar_bits & 0xFu)) << 0xE;
    masks = (masks | (static_cast<u32>(instruction.output_flag) & 1u)) << 2 | (head & 0xF0F0FFFBu);
    const u32 output_index = instruction.output_register != nullptr ? instruction.output_register->source : 0;
    const u32 destination = select_shader_validation_destination_index(instruction);
    frame.word0 = ((destination & 0xFu) << 0x11 | (output_index & 0x1FFu)) << 3 | (masks & 0xFF0FF007u);

    output[0] = frame.word0;
    output[1] = frame.word1;
    output[2] = frame.word2;
    return output;
}

// Xbox 0x001914E8
bool serialize_shader_validation_program(ShaderValidationListNode* first, u32* program,
    ShaderValidationSerializerFrame& frame, const ShaderValidationOwner* active_owner)
{
    program[0] = 0;
    for (ShaderValidationListNode* node = first; node != nullptr; node = node->next)
    {
        u32 words[3];
        const u32* packed = serialize_shader_validation_instruction(
            *static_cast<ShaderValidationInstruction*>(node->value), words, frame, active_owner);
        u32* destination = program + program[0] * 3 + 1;
        destination[0] = packed[0];
        destination[1] = packed[1];
        destination[2] = packed[2];
        ++program[0];
    }
    resolve_shader_validation_reserved_register(program);
    return true;
}
}
