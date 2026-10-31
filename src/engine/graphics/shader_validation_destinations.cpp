#include "shader_validation_destinations.h"
#include "gbtg/host_seh.h"
#include "shader_validation_channel_ops.h"
#include "shader_validation_cleanup.h"
#include "shader_validation_instruction_ops.h"

namespace gbtg
{
// Xbox 0x00190D77
bool merge_shader_validation_duplicate_channel(ShaderValidationOwner& owner, u32 first, u32 second,
    const std::function<ShaderValidationOperand*(u32)>& operand_at)
{
    if (!shader_validation_owner_channels_share_writer(owner, first, second))
    {
        return false;
    }
    ShaderValidationOwner* ancestor = owner.channel_owner;
    while (ancestor != nullptr)
    {
        if (ancestor->channels[second] != 0)
        {
            break;
        }
        if (ancestor->channels[first] != 0)
        {
            const u32 original_first = first;
            first = second;
            second = original_first;
            break;
        }
        ancestor = ancestor->channel_owner;
    }
    const auto writer = owner.channels[first];
    if (writer != shader_validation_unwritten_channel)
    {
        remove_shader_validation_destination_channel(*reinterpret_cast<ShaderValidationInstruction*>(writer), second, &owner);
    }
    ShaderValidationOwner* current = &owner;
    do
    {
        if (current->channels[second] != 0)
        {
            break;
        }
        ShaderValidationBalancedNode* node;
        find_shader_validation_tree_last(current->diagnostics.nodes, &node);
        while (node != nullptr)
        {
            for (u32 channel = 0; channel < 4; ++channel)
            {
                ShaderValidationOperand* operand = operand_at(node->value);
                if (operand->channel_order.channels[channel] == second)
                {
                    // Reload the operand just as retail reloads the node value for the store.
                    operand_at(node->value)->channel_order.channels[channel] = first;
                }
            }
            advance_shader_validation_balanced_iterator(&node);
        }
        current = current->channel_owner;
    } while (current != nullptr);
    return true;
}

// Xbox 0x00191C47
void merge_shader_validation_duplicate_channels(ShaderValidationOwner& owner,
    const std::function<ShaderValidationOperand*(u32)>& operand_at)
{
    for (u32 first = 0; first < 4; ++first)
    {
        for (u32 second = first + 1; second < 4; ++second)
        {
            merge_shader_validation_duplicate_channel(owner, first, second, operand_at);
        }
    }
}

// Xbox 0x00190D50
void set_shader_validation_channel_writer(ShaderValidationOwner& owner, u32 channel,
    ShaderValidationChannelWriter writer)
{
    auto& slot = owner.channels[channel];
    const auto previous = slot;
    if (previous != 0 && previous != shader_validation_unwritten_channel)
    {
        remove_shader_validation_destination_channel(*reinterpret_cast<ShaderValidationInstruction*>(previous), channel, &owner);
    }
    slot = writer;
}

// Xbox 0x001908FC
void remove_shader_validation_destination_channel(ShaderValidationInstruction& instruction, u32 channel,
    ShaderValidationOwner* owner)
{
    if (owner == instruction.output_register)
    {
        if (instruction.output_mask.channels[channel] != 0)
        {
            instruction.output_mask.channels[channel] = 0;
            set_shader_validation_channel_writer(*instruction.output_register, channel, 0);
        }
        if (pack_shader_validation_channel_mask(instruction.output_mask) == 0)
        {
            set_shader_validation_output_destination(instruction, nullptr);
        }
    }
    else if (owner == instruction.vector_destination)
    {
        if (instruction.vector_mask.channels[channel] != 0)
        {
            instruction.vector_mask.channels[channel] = 0;
            set_shader_validation_channel_writer(*instruction.vector_destination, channel, 0);
        }
        if (pack_shader_validation_channel_mask(instruction.vector_mask) == 0)
        {
            set_shader_validation_vector_destination(instruction, nullptr);
        }
    }
    else if (owner == instruction.scalar_destination)
    {
        if (instruction.scalar_mask.channels[channel] != 0)
        {
            instruction.scalar_mask.channels[channel] = 0;
            set_shader_validation_channel_writer(*instruction.scalar_destination, channel, 0);
        }
        if (pack_shader_validation_channel_mask(instruction.scalar_mask) == 0)
        {
            set_shader_validation_scalar_destination(instruction, nullptr);
        }
    }
}

namespace
{
void replace_destination_body(ShaderValidationInstruction& instruction, ShaderValidationOwner* owner,
    ShaderValidationChannelMask& argument, ShaderValidationOwner*& destination, ShaderValidationChannelMask& destination_mask,
    ShaderValidationChannelMask& temporary, i32& state)
{
    ShaderValidationOwner* previous = destination;
    state = 0;
    if (previous != nullptr)
    {
        temporary.vtable_address = 0x00203790;
        temporary.channels = destination_mask.channels;
        state = 1;
        for (u32 channel = 0; channel < 4; ++channel)
        {
            if (temporary.channels[channel] != 0)
            {
                set_shader_validation_channel_writer(*previous, channel, 0);
            }
        }
        state = 0;
    }
    temporary.vtable_address = 0x00203790;
    if (owner != nullptr)
    {
        temporary.channels = argument.channels;
        state = 2;
        for (u32 channel = 0; channel < 4; ++channel)
        {
            if (temporary.channels[channel] != 0)
            {
                set_shader_validation_channel_writer(*owner, channel, reinterpret_cast<ShaderValidationChannelWriter>(&instruction));
            }
        }
        copy_shader_validation_channel_mask(destination_mask, argument);
    }
    else
    {
        decode_shader_validation_channel_mask(temporary, 0);
        copy_shader_validation_channel_mask(destination_mask, temporary);
    }
    destination = owner;
}
}

// Xbox 0x0018F608
ShaderValidationOwner* replace_shader_validation_output_destination(ShaderValidationInstruction& instruction,
    ShaderValidationOwner* owner, ShaderValidationChannelMask mask)
{
    i32 state = -1;
    ShaderValidationChannelMask temporary;
    GBTG_SEH_TRY_FINALLY
    {
        replace_destination_body(instruction, owner, mask, instruction.output_register,
            instruction.output_mask, temporary, state);
    }
    GBTG_SEH_FINALLY
    {
        if (GBTG_SEH_ABNORMAL())
        {
            // FuncInfo 0x0032B7CC: states 1/2 destroy EBP-0x18, then state 0 destroys EBP+0x0c.
            if (state == 1 || state == 2)
            {
                destroy_shader_validation_channel_mask(temporary);
            }
            if (state >= 0)
            {
                destroy_shader_validation_channel_mask(mask);
            }
        }
    }
    GBTG_SEH_FINALLY_END
    return owner;
}

// Xbox 0x0019070B
void set_shader_validation_output_destination(ShaderValidationInstruction& instruction, ShaderValidationOwner* owner)
{
    replace_shader_validation_output_destination(instruction, owner, ShaderValidationChannelMask{0x00203790, instruction.output_mask.channels});
}

// Xbox 0x0018F6B7
ShaderValidationOwner* replace_shader_validation_vector_destination(ShaderValidationInstruction& instruction,
    ShaderValidationOwner* owner, ShaderValidationChannelMask mask)
{
    i32 state = -1;
    ShaderValidationChannelMask temporary;
    GBTG_SEH_TRY_FINALLY
    {
        replace_destination_body(instruction, owner, mask, instruction.vector_destination,
            instruction.vector_mask, temporary, state);
    }
    GBTG_SEH_FINALLY
    {
        if (GBTG_SEH_ABNORMAL())
        {
            // FuncInfo 0x0032B7CC: states 1/2 destroy EBP-0x18, then state 0 destroys EBP+0x0c.
            if (state == 1 || state == 2)
            {
                destroy_shader_validation_channel_mask(temporary);
            }
            if (state >= 0)
            {
                destroy_shader_validation_channel_mask(mask);
            }
        }
    }
    GBTG_SEH_FINALLY_END
    return owner;
}

// Xbox 0x0019072E
void set_shader_validation_vector_destination(ShaderValidationInstruction& instruction, ShaderValidationOwner* owner)
{
    replace_shader_validation_vector_destination(instruction, owner, ShaderValidationChannelMask{0x00203790, instruction.vector_mask.channels});
}

// Xbox 0x0018F766
ShaderValidationOwner* replace_shader_validation_scalar_destination(ShaderValidationInstruction& instruction,
    ShaderValidationOwner* owner, ShaderValidationChannelMask mask)
{
    i32 state = -1;
    ShaderValidationChannelMask temporary;
    GBTG_SEH_TRY_FINALLY
    {
        replace_destination_body(instruction, owner, mask, instruction.scalar_destination,
            instruction.scalar_mask, temporary, state);
    }
    GBTG_SEH_FINALLY
    {
        if (GBTG_SEH_ABNORMAL())
        {
            // FuncInfo 0x0032B7CC: states 1/2 destroy EBP-0x18, then state 0 destroys EBP+0x0c.
            if (state == 1 || state == 2)
            {
                destroy_shader_validation_channel_mask(temporary);
            }
            if (state >= 0)
            {
                destroy_shader_validation_channel_mask(mask);
            }
        }
    }
    GBTG_SEH_FINALLY_END
    return owner;
}

// Xbox 0x00190751
void set_shader_validation_scalar_destination(ShaderValidationInstruction& instruction, ShaderValidationOwner* owner)
{
    replace_shader_validation_scalar_destination(instruction, owner, ShaderValidationChannelMask{0x00203790, instruction.scalar_mask.channels});
}

// Xbox 0x00191C0B
void set_shader_validation_masked_writers(ShaderValidationOwner& owner, ShaderValidationChannelMask mask,
    ShaderValidationChannelWriter writer)
{
    GBTG_SEH_TRY_FINALLY
    {
        for (u32 channel = 0; channel < 4; ++channel)
        {
            if (mask.channels[channel] != 0)
            {
                set_shader_validation_channel_writer(owner, channel, writer);
            }
        }
    }
    GBTG_SEH_FINALLY
    {
        if (GBTG_SEH_ABNORMAL())
        {
            // FuncInfo 0x0032B848, state 0: 0x00170823 destroys EBP+8.
            destroy_shader_validation_channel_mask(mask);
        }
    }
    GBTG_SEH_FINALLY_END
}
}
