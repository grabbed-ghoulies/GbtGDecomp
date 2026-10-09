#include "shader_validation_instruction_ops.h"
#include <bit>

namespace gbtg
{
// Xbox 0x0018E988
bool shader_validation_instruction_uses_slot(const ShaderValidationInstruction& instruction, u32 slot)
{
    if (slot == 0)
    {
        return instruction.vector_opcode != 0;
    }
    if (slot == 1)
    {
        const u32 opcode = instruction.vector_opcode;
        if (opcode == 0 || opcode == 3 || opcode == 0xD || opcode == 1)
        {
            return false;
        }
        return true;
    }
    if (slot == 2)
    {
        if (instruction.scalar_opcode != 0 || instruction.vector_opcode == 3 || instruction.vector_opcode == 4)
        {
            return true;
        }
        return false;
    }
    return false;
}

// Xbox 0x0018ED95
bool negate_shader_validation_move_source(ShaderValidationInstruction& instruction,
    const ShaderValidationOwner* destination)
{
    ShaderValidationOperand* operand;
    if (destination == instruction.vector_destination && instruction.vector_opcode == 1)
    {
        if (instruction.output_register != nullptr && instruction.output_flag == 0)
        {
            return false;
        }
        operand = instruction.operands[0];
    }
    else
    {
        if (destination != instruction.scalar_destination || instruction.scalar_opcode != 1)
        {
            return false;
        }
        if (instruction.vector_opcode == 3 || instruction.vector_opcode == 4)
        {
            return false;
        }
        if (instruction.output_register != nullptr && instruction.output_flag != 0)
        {
            return false;
        }
        operand = instruction.operands[2];
    }
    operand->marker = operand->marker == 0 ? 1 : 0;
    return true;
}

// Xbox 0x0018FF4B
bool convert_shader_validation_scalar_move(ShaderValidationInstruction& instruction)
{
    if (instruction.vector_opcode != 0 || instruction.scalar_opcode != 1)
    {
        return false;
    }
    instruction.vector_opcode = 1;
    instruction.scalar_opcode = 0;
    copy_shader_validation_channel_mask(instruction.vector_mask, instruction.scalar_mask);
    ShaderValidationChannelMask empty;
    empty.vtable_address = 0x00203790;
    decode_shader_validation_channel_mask(empty, 0);
    copy_shader_validation_channel_mask(instruction.scalar_mask, empty);
    ShaderValidationOperand* scalar_source = instruction.operands[2];
    instruction.vector_destination = instruction.scalar_destination;
    instruction.output_flag = instruction.output_flag == 0 ? 1 : 0;
    instruction.operands[2] = instruction.operands[0];
    instruction.scalar_destination = nullptr;
    instruction.operands[0] = scalar_source;
    scalar_source->kind = 0;
    instruction.operands[2]->kind = 2;
    return true;
}

namespace
{
bool vector_writes(const ShaderValidationInstruction& instruction, i32 channel)
{
    return shader_validation_vector_writes_channel(instruction, static_cast<u32>(channel)) != 0;
}

bool scalar_writes(const ShaderValidationInstruction& instruction, i32 channel)
{
    return shader_validation_scalar_writes_channel(instruction, static_cast<u32>(channel)) != 0;
}

// The scalar-unit tail shared by every vector opcode case (0x0018EA0C).
bool scalar_slot_reads_channel(const ShaderValidationInstruction& instruction, u32 slot, i32 channel)
{
    if (slot != 2)
    {
        return false;
    }
    switch (instruction.scalar_opcode)
    {
    case 0:
        return false;
    case 1:
        return scalar_writes(instruction, channel);
    case 6:
        if (scalar_writes(instruction, 0) && channel == 0)
        {
            return true;
        }
        if (scalar_writes(instruction, 1) && channel == 0)
        {
            return true;
        }
        if (!scalar_writes(instruction, 2))
        {
            return false;
        }
        return channel == 0;
    case 7:
        if (scalar_writes(instruction, 2) && channel != 2)
        {
            return true;
        }
        if (!scalar_writes(instruction, 1))
        {
            return false;
        }
        return channel == 0;
    default:
        return channel == 0;
    }
}

// Default vector case (0x0018EA03): slots zero and one read written vector channels.
bool vector_slot_reads_channel(const ShaderValidationInstruction& instruction, u32 slot, i32 channel)
{
    if ((slot == 0 || slot == 1) && vector_writes(instruction, channel))
    {
        return true;
    }
    return scalar_slot_reads_channel(instruction, slot, channel);
}
}

// Xbox 0x0018E9DC
bool shader_validation_instruction_reads_channel(const ShaderValidationInstruction& instruction, u32 slot, i32 channel)
{
    const i32 opcode = std::bit_cast<i32>(instruction.vector_opcode);
    if (opcode > 5)
    {
        if (opcode <= 7)
        {
            if (slot == 0 || slot == 1)
            {
                return true;
            }
            return scalar_slot_reads_channel(instruction, slot, channel);
        }
        if (opcode == 8)
        {
            if (slot == 0)
            {
                if (channel == 1 || channel == 2)
                {
                    return vector_writes(instruction, channel);
                }
                return false;
            }
            if (slot == 1)
            {
                if (channel == 1 || channel == 3)
                {
                    return vector_writes(instruction, channel);
                }
                return false;
            }
            return scalar_slot_reads_channel(instruction, slot, channel);
        }
        if (opcode == 0xD)
        {
            if (slot != 0)
            {
                return scalar_slot_reads_channel(instruction, slot, channel);
            }
            return channel == 0;
        }
        return vector_slot_reads_channel(instruction, slot, channel);
    }
    if (opcode == 5)
    {
        if ((slot == 0 || slot == 1) && channel < 3)
        {
            return true;
        }
        return scalar_slot_reads_channel(instruction, slot, channel);
    }
    switch (opcode)
    {
    case 0:
        return scalar_slot_reads_channel(instruction, slot, channel);
    case 1:
        if (slot != 0)
        {
            return scalar_slot_reads_channel(instruction, slot, channel);
        }
        return vector_writes(instruction, channel);
    case 3:
        if (slot == 0 || slot == 2)
        {
            if (vector_writes(instruction, channel))
            {
                return true;
            }
            return scalar_slot_reads_channel(instruction, slot, channel);
        }
        return false;
    case 4:
        if (vector_writes(instruction, channel))
        {
            return true;
        }
        return scalar_slot_reads_channel(instruction, slot, channel);
    default:
        return vector_slot_reads_channel(instruction, slot, channel);
    }
}

// Xbox 0x0018F848
bool shader_validation_operand_reads_channel(const ShaderValidationOperand& operand, i32 channel)
{
    if (operand.kind == 3)
    {
        return channel == 0;
    }
    return shader_validation_instruction_reads_channel(*operand.instruction, operand.kind, channel);
}

// Xbox 0x0018EB4C
ShaderValidationChannelMask* shader_validation_instruction_read_mask(const ShaderValidationInstruction& instruction,
    ShaderValidationChannelMask& result, u32 slot)
{
    const u8 fourth = shader_validation_instruction_reads_channel(instruction, slot, 3) ? 1 : 0;
    const u8 third = shader_validation_instruction_reads_channel(instruction, slot, 2) ? 1 : 0;
    const u8 second = shader_validation_instruction_reads_channel(instruction, slot, 1) ? 1 : 0;
    const u8 first = shader_validation_instruction_reads_channel(instruction, slot, 0) ? 1 : 0;
    construct_shader_validation_channel_mask(result, first, second, third, fourth);
    return &result;
}

// Xbox 0x0018EB96
bool shader_validation_instruction_reads_unset_channel(const ShaderValidationInstruction& instruction)
{
    u32 slot = 0;
    do
    {
        const ShaderValidationOwner* owner = instruction.operands[slot]->parent;
        if (owner != nullptr)
        {
            i32 channel = 0;
            do
            {
                if (shader_validation_instruction_reads_channel(instruction, slot, channel) &&
                    owner->channels[static_cast<std::size_t>(channel)] == shader_validation_unwritten_channel)
                {
                    return true;
                }
                ++channel;
            } while (channel <= 3);
        }
        ++slot;
    } while (slot <= 2);
    return false;
}

namespace
{
// True when any channel of the owner is written by an instruction whose scalar destination is that owner.
bool owner_has_scalar_writer(const ShaderValidationOwner& owner)
{
    bool found = false;
    for (const ShaderValidationChannelWriter writer : owner.channels)
    {
        if (writer != 0 && writer != shader_validation_unwritten_channel &&
            reinterpret_cast<const ShaderValidationInstruction*>(writer)->scalar_destination == &owner)
        {
            found = true;
        }
    }
    return found;
}
}

// Xbox 0x0018FFC9
bool convert_shader_validation_vector_move(ShaderValidationInstruction& instruction, u8 check_writers)
{
    // check_writers is the retail validator phase byte DAT_001a4d33.
    if (instruction.scalar_opcode != 0 || instruction.vector_opcode != 1)
    {
        return false;
    }
    if (check_writers != 0 && instruction.vector_destination != nullptr)
    {
        bool found = false;
        for (const ShaderValidationOwner* owner = instruction.vector_destination; owner != nullptr;)
        {
            found = owner_has_scalar_writer(*owner) || found;
            owner = owner->derived_owner;
            if (found)
            {
                break;
            }
        }
        if (!found)
        {
            for (const ShaderValidationOwner* owner = instruction.vector_destination; owner != nullptr;)
            {
                found = owner_has_scalar_writer(*owner) || found;
                owner = owner->channel_owner;
                if (found)
                {
                    break;
                }
            }
            if (!found)
            {
                return false;
            }
        }
    }
    instruction.scalar_opcode = 1;
    instruction.vector_opcode = 0;
    copy_shader_validation_channel_mask(instruction.scalar_mask, instruction.vector_mask);
    ShaderValidationChannelMask empty;
    empty.vtable_address = 0x00203790;
    decode_shader_validation_channel_mask(empty, 0);
    copy_shader_validation_channel_mask(instruction.vector_mask, empty);
    ShaderValidationOperand* vector_source = instruction.operands[2];
    instruction.scalar_destination = instruction.vector_destination;
    instruction.output_flag = instruction.output_flag == 0 ? 1 : 0;
    instruction.operands[2] = instruction.operands[0];
    instruction.vector_destination = nullptr;
    instruction.operands[0] = vector_source;
    vector_source->kind = 0;
    instruction.operands[2]->kind = 2;
    return true;
}

// Xbox 0x001906BB
bool shader_validation_operand_reads_order_channel(const ShaderValidationOperand& operand, u32 channel)
{
    i32 index = 0;
    do
    {
        const u32 value = index == 4 ? 4u : operand.channel_order.channels[static_cast<std::size_t>(index)];
        if (value == channel && shader_validation_operand_reads_channel(operand, index))
        {
            return true;
        }
        ++index;
    } while (index <= 3);
    return false;
}

namespace
{
// One owner search of 0x0018FC2A/0x0018FD7D: the writer's mask, restricted to the channels the
// reader's slot sources, is followed through channel owners until a channel owner overwrites it.
bool result_reaches_slot(const ShaderValidationInstruction& reader, u32 slot, const ShaderValidationOwner* owner,
    const ShaderValidationChannelMask& written, ShaderValidationChannelMask& remaining)
{
    ShaderValidationChannelMask read;
    shader_validation_instruction_read_mask(reader, read, slot);
    ShaderValidationChannelMask sources;
    shader_validation_operand_source_mask(*reader.operands[slot], read, sources);
    ShaderValidationChannelMask reached;
    intersect_shader_validation_channel_masks(written, sources, reached);
    copy_shader_validation_channel_mask(remaining, reached);
    while (pack_shader_validation_channel_mask(remaining) != 0)
    {
        if (owner == nullptr)
        {
            return false;
        }
        if (reader.operands[slot]->parent == owner)
        {
            return true;
        }
        owner = owner->channel_owner;
        if (owner != nullptr)
        {
            for (std::size_t channel = 0; channel < 4; ++channel)
            {
                if (owner->channels[channel] != 0)
                {
                    remaining.channels[channel] = 0;
                }
            }
        }
    }
    return false;
}
}

// Xbox 0x0018FC2A
bool shader_validation_vector_result_reaches(const ShaderValidationInstruction& reader, u32 slot,
    const ShaderValidationInstruction& writer)
{
    ShaderValidationChannelMask remaining;
    construct_shader_validation_channel_mask(remaining, 0xF);
    ShaderValidationChannelMask written;
    if (writer.output_flag == 0)
    {
        copy_shader_validation_output_mask(writer, written);
        if (result_reaches_slot(reader, slot, writer.output_register, written, remaining))
        {
            return true;
        }
    }
    copy_shader_validation_vector_mask(writer, written);
    return result_reaches_slot(reader, slot, writer.vector_destination, written, remaining);
}

// Xbox 0x0018FD7D
bool shader_validation_scalar_result_reaches(const ShaderValidationInstruction& reader, u32 slot,
    const ShaderValidationInstruction& writer)
{
    ShaderValidationChannelMask remaining;
    construct_shader_validation_channel_mask(remaining, 0xF);
    ShaderValidationChannelMask written;
    if (writer.output_flag != 0)
    {
        copy_shader_validation_output_mask(writer, written);
        if (result_reaches_slot(reader, slot, writer.output_register, written, remaining))
        {
            return true;
        }
    }
    copy_shader_validation_scalar_mask(writer, written);
    return result_reaches_slot(reader, slot, writer.scalar_destination, written, remaining);
}

namespace
{
u32 operand_channel(const ShaderValidationInstruction& instruction, std::size_t slot, u32 index)
{
    return index == 4 ? 4u : instruction.operands[slot]->channel_order.channels[index];
}

bool operand_channels_match(const ShaderValidationInstruction& instruction, std::size_t slot, u32 first, u32 second)
{
    return operand_channel(instruction, slot, first) == operand_channel(instruction, slot, second);
}

void swap_operand_channels(ShaderValidationInstruction& instruction, std::size_t slot, u32 first, u32 second)
{
    const u32 first_value = operand_channel(instruction, slot, first);
    const u32 second_value = operand_channel(instruction, slot, second);
    instruction.operands[slot]->channel_order.channels[first] = second_value;
    instruction.operands[slot]->channel_order.channels[second] = first_value;
}

void swap_mask_channels(ShaderValidationChannelMask& mask, u32 first, u32 second)
{
    if (mask.channels[first] != mask.channels[second])
    {
        mask.channels[first] = mask.channels[first] == 0 ? 1 : 0;
        mask.channels[second] = mask.channels[second] == 0 ? 1 : 0;
    }
}
}

// Xbox 0x0018EBEA
bool shader_validation_destination_channels_match(const ShaderValidationInstruction& instruction,
    const ShaderValidationOwner* destination, u32 first, u32 second)
{
    if (destination == instruction.vector_destination)
    {
        const i32 opcode = std::bit_cast<i32>(instruction.vector_opcode);
        if (opcode > 4)
        {
            if (opcode <= 7)
            {
                return true;
            }
            if (opcode == 8 || opcode == 0xD)
            {
                return false;
            }
        }
        else if (opcode == 4)
        {
            return operand_channels_match(instruction, 0, first, second) && operand_channels_match(instruction, 1, first, second) &&
                   operand_channels_match(instruction, 2, first, second);
        }
        else if (opcode == 0)
        {
            return true;
        }
        else if (opcode == 1)
        {
            return operand_channels_match(instruction, 0, first, second);
        }
        else if (opcode == 3)
        {
            return operand_channels_match(instruction, 0, first, second) && operand_channels_match(instruction, 2, first, second);
        }
        return operand_channels_match(instruction, 0, first, second) && operand_channels_match(instruction, 1, first, second);
    }
    if (destination != instruction.scalar_destination)
    {
        return false;
    }
    const i32 opcode = std::bit_cast<i32>(instruction.scalar_opcode);
    if (opcode == 0)
    {
        return true;
    }
    if (opcode == 1)
    {
        return operand_channels_match(instruction, 2, first, second);
    }
    return opcode > 1 && opcode <= 4;
}

// Xbox 0x0018EDEE
bool swap_shader_validation_destination_channels(ShaderValidationInstruction& instruction,
    const ShaderValidationOwner* destination, u32 first, u32 second, u8 query_only)
{
    if (destination == instruction.vector_destination)
    {
        if (instruction.output_register != nullptr && instruction.output_flag == 0)
        {
            return false;
        }
        const i32 opcode = std::bit_cast<i32>(instruction.vector_opcode);
        if (opcode >= 5)
        {
            if (opcode <= 7)
            {
                if (query_only == 0)
                {
                    swap_mask_channels(instruction.vector_mask, first, second);
                }
                return true;
            }
            if (opcode == 8 || opcode == 0xD)
            {
                return false;
            }
        }
        if (shader_validation_vector_opcode_is_add_or_mad(instruction) != 0 && instruction.scalar_opcode != 0)
        {
            return false;
        }
        if (query_only != 0)
        {
            return true;
        }
        swap_mask_channels(instruction.vector_mask, first, second);
        for (u32 slot = 0; slot <= 2; ++slot)
        {
            if (shader_validation_instruction_uses_slot(instruction, slot) && (instruction.scalar_opcode == 0 || slot != 2))
            {
                swap_operand_channels(instruction, slot, first, second);
            }
        }
        return true;
    }
    if (destination != instruction.scalar_destination)
    {
        return false;
    }
    if (instruction.output_register != nullptr && instruction.output_flag != 0)
    {
        return false;
    }
    const i32 opcode = std::bit_cast<i32>(instruction.scalar_opcode);
    if (opcode == 1)
    {
        if (shader_validation_vector_opcode_is_add_or_mad(instruction) != 0)
        {
            return false;
        }
        if (query_only != 0)
        {
            return true;
        }
        swap_mask_channels(instruction.scalar_mask, first, second);
        swap_operand_channels(instruction, 2, first, second);
        return true;
    }
    if (opcode < 1 || opcode > 4)
    {
        return false;
    }
    if (query_only == 0)
    {
        swap_mask_channels(instruction.scalar_mask, first, second);
    }
    return true;
}

// Xbox 0x001907E9
bool shader_validation_operand_reads_masked_channel(const ShaderValidationOperand& operand, ShaderValidationChannelMask mask)
{
    i32 channel = 0;
    do
    {
        if (mask.channels[static_cast<std::size_t>(channel)] != 0 &&
            shader_validation_operand_reads_order_channel(operand, static_cast<u32>(channel)))
        {
            return true;
        }
        ++channel;
    } while (channel <= 3);
    return false;
}

// Xbox 0x0018F815
bool shader_validation_owner_channels_share_writer(const ShaderValidationOwner& owner, u32 first, u32 second)
{
    // Channel indices are 0..3 at every retail call site.
    const ShaderValidationChannelWriter writer = owner.channels[first];
    if (writer != owner.channels[second] || writer == 0 || writer == shader_validation_unwritten_channel)
    {
        return false;
    }
    return shader_validation_destination_channels_match(*reinterpret_cast<const ShaderValidationInstruction*>(writer), &owner, first,
        second);
}

// Xbox 0x001900DE
ShaderValidationBalancedNode** find_shader_validation_tree_last(const ShaderValidationBalancedTree& tree,
    ShaderValidationBalancedNode** result)
{
    // Typed overload for balanced-node registries, which share the +0x0C first link.
    ShaderValidationBalancedNode* node = tree.root;
    ShaderValidationBalancedNode* last = tree.root;
    while (node != nullptr)
    {
        last = node;
        node = node->first;
    }
    *result = last;
    return result;
}

// Xbox 0x00190D14
bool shader_validation_owner_diagnostic_reads_channel(const ShaderValidationOwner& owner, u32 channel,
    const std::function<const ShaderValidationOperand*(u32)>& operand_at)
{
    // Registry nodes hold retail operand addresses; operand_at maps them to the actual operands.
    ShaderValidationBalancedNode* node;
    find_shader_validation_tree_last(owner.diagnostics.nodes, &node);
    while (node != nullptr)
    {
        if (shader_validation_operand_reads_order_channel(*operand_at(node->value), channel))
        {
            return true;
        }
        advance_shader_validation_balanced_iterator(&node);
    }
    return false;
}

// Xbox 0x00190858
bool shader_validation_instruction_depends_on(const ShaderValidationInstruction& reader, const ShaderValidationInstruction& writer,
    const ShaderValidationOwner* active_owner)
{
    u32 slot = 0;
    do
    {
        if (shader_validation_vector_result_reaches(reader, slot, writer) || shader_validation_scalar_result_reaches(reader, slot, writer))
        {
            return true;
        }
        const ShaderValidationOwner* parent = reader.operands[slot]->parent;
        if (parent != nullptr && find_shader_validation_foreign_parent(*parent, active_owner) == writer.vector_destination)
        {
            return true;
        }
        ++slot;
    } while (slot <= 2);
    return false;
}

// Xbox 0x0018FED0
bool shader_validation_instructions_can_pair(const ShaderValidationInstruction& reader, const ShaderValidationInstruction& writer)
{
    bool vector_free = true;
    bool scalar_free = true;
    u32 slot = 0;
    do
    {
        if (shader_validation_vector_result_reaches(reader, slot, writer) &&
            (shader_validation_vector_opcode_is_add_through_dp4(writer) != 0 || writer.scalar_opcode != 0) &&
            (slot != 2 || reader.scalar_opcode != 0))
        {
            vector_free = false;
        }
        if (shader_validation_scalar_result_reaches(reader, slot, writer) && (slot != 2 || reader.scalar_opcode != 0))
        {
            scalar_free = false;
        }
        ++slot;
    } while (slot <= 2);
    return vector_free && scalar_free;
}

// Xbox 0x001908AE
bool shader_validation_instruction_must_follow(const ShaderValidationInstruction& reader, const ShaderValidationInstruction& writer,
    const ShaderValidationOwner* active_owner)
{
    if (!shader_validation_instruction_depends_on(reader, writer, active_owner))
    {
        return false;
    }
    if (writer.vector_opcode == 0xD)
    {
        // An ARL writer orders every reader with a foreign operand parent; parents are read without a null test.
        for (u32 slot = 0; slot <= 2; ++slot)
        {
            if (find_shader_validation_foreign_parent(*reader.operands[slot]->parent, active_owner) != nullptr)
            {
                return true;
            }
        }
    }
    return !shader_validation_instructions_can_pair(reader, writer);
}
}
