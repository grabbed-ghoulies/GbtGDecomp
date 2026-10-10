#include "shader_validation_instruction_constructor.h"
#include "shader_validation_cleanup.h"
#include "shader_validation_instruction_ops.h"
#include "shader_validation_destinations.h"
#if defined(_WIN32)
#include <excpt.h>
#else
#error Shader validator constructor unwind requires the approved Windows SEH backend.
#endif

namespace gbtg
{
// Xbox 0x0019373D
bool clear_unused_shader_validation_operand_channels(ShaderValidationConstructorContext& context,
    ShaderValidationOperand& operand)
{
    bool changed = false;
    for (u32 channel = 0; channel < 4; ++channel)
    {
        if (!shader_validation_operand_reads_channel(operand, static_cast<i32>(channel)))
        {
            operand.channel_order.channels[channel] = 4;
            changed = true;
        }
    }
    ShaderValidationChannelMask used;
    shader_validation_channel_order_used_mask(operand.channel_order, used);
    if (pack_shader_validation_channel_mask(used) == 0)
    {
        reparent_shader_validation_operand(context, operand, context.active_owner);
    }
    return changed;
}

// Xbox 0x0019379C
bool clear_unused_shader_validation_instruction_channels(ShaderValidationConstructorContext& context,
    ShaderValidationInstruction& instruction)
{
    std::array<bool, 3> changed;
    for (u32 slot = 0; slot < 3; ++slot)
    {
        changed[slot] = clear_unused_shader_validation_operand_channels(context, *instruction.operands[slot]);
    }
    if (!changed[0] && !changed[1] && !changed[2])
    {
        return false;
    }
    return true;
}


namespace
{
ShaderValidationInstruction* split_scalar_body(ShaderValidationConstructorContext& context, ShaderValidationInstruction& source,
    i32& state, void*& first_allocation, void*& second_allocation, void*& third_allocation, void*& fourth_allocation)
{
    first_allocation = nullptr;
    if (source.vector_opcode == 0 || source.scalar_opcode == 0)
    {
        return nullptr;
    }
    first_allocation = context.allocate(sizeof(ShaderValidationInstruction), 0x24870000);
    state = 0;
    ShaderValidationInstruction* result;
    if (first_allocation == nullptr)
    {
        result = nullptr;
    }
    else
    {
        result = construct_shader_validation_instruction_base(context.objects, *static_cast<ShaderValidationInstruction*>(first_allocation),
            context.retail_address(first_allocation));
    }
    result->instruction_set_count = 0xFFFFFFFF;
    result->sequence = source.sequence;
    result->instruction_set = nullptr;
    result->vector_opcode = 0;
    result->scalar_opcode = source.scalar_opcode;
    source.scalar_opcode = 0;
    ShaderValidationChannelMask empty;
    empty.vtable_address = 0x00203790;
    if (source.output_flag == 0)
    {
        decode_shader_validation_channel_mask(empty, 0);
        copy_shader_validation_channel_mask(result->output_mask, empty);
    }
    else
    {
        copy_shader_validation_channel_mask(result->output_mask, source.output_mask);
    }
    decode_shader_validation_channel_mask(empty, 0);
    copy_shader_validation_channel_mask(result->vector_mask, empty);
    // Retail overwrites EBP-0x18 with this mask address after state 0's base construction.
    first_allocation = &source.scalar_mask;
    copy_shader_validation_channel_mask(result->scalar_mask, source.scalar_mask);
    result->output_flag = source.output_flag;
    for (u32 slot = 0; slot < 2; ++slot)
    {
        void*& operand_allocation = slot == 0 ? second_allocation : third_allocation;
        void*& owner_allocation = slot == 0 ? third_allocation : second_allocation;
        operand_allocation = context.allocate(sizeof(ShaderValidationOperand), 0x24870000);
        state = slot == 0 ? 1 : 3;
        ShaderValidationOperand* operand;
        if (operand_allocation == nullptr)
        {
            operand = nullptr;
        }
        else
        {
            owner_allocation = context.allocate(sizeof(ShaderValidationOwner), 0x24870000);
            state = slot == 0 ? 2 : 4;
            ShaderValidationOwner* owner;
            if (owner_allocation == nullptr)
            {
                owner = nullptr;
            }
            else
            {
                ShaderValidationChannelMask full;
                construct_shader_validation_channel_mask(full, 0xF);
                owner = construct_shader_validation_owner(context, *static_cast<ShaderValidationOwner*>(owner_allocation),
                    context.retail_address(owner_allocation), 0, 0, nullptr, 0, full);
            }
            state = slot == 0 ? 1 : 3;
            operand = construct_shader_validation_operand(context, *static_cast<ShaderValidationOperand*>(operand_allocation),
                context.retail_address(operand_allocation), result, nullptr, slot, owner, 0);
        }
        result->operands[slot] = operand;
    }
    fourth_allocation = context.allocate(sizeof(ShaderValidationOperand), 0x24870000);
    state = 5;
    ShaderValidationOperand* scalar;
    if (fourth_allocation == nullptr)
    {
        scalar = nullptr;
    }
    else
    {
        scalar = construct_shader_validation_operand(context, *static_cast<ShaderValidationOperand*>(fourth_allocation),
            context.retail_address(fourth_allocation), result, nullptr, 2, source.operands[2]->parent, source.operands[2]->marker);
    }
    state = -1;
    result->operands[2] = scalar;
    ShaderValidationChannelOrder original;
    copy_shader_validation_operand_order(original, *source.operands[2]);
    ShaderValidationChannelOrder ignored;
    replace_shader_validation_operand_channel_order(ignored, *result->operands[2], original);
    if (!shader_validation_vector_opcode_is_add_or_mad(source))
    {
        second_allocation = context.allocate(sizeof(ShaderValidationOwner), 0x24870000);
        state = 6;
        ShaderValidationOwner* owner;
        if (second_allocation == nullptr)
        {
            owner = nullptr;
        }
        else
        {
            ShaderValidationChannelMask full;
            construct_shader_validation_channel_mask(full, 0xF);
            owner = construct_shader_validation_owner(context, *static_cast<ShaderValidationOwner*>(second_allocation),
                context.retail_address(second_allocation), 0, 0, nullptr, 0, full);
        }
        state = -1;
        reparent_shader_validation_operand(context, *source.operands[2], owner);
    }
    result->scalar_destination = nullptr;
    result->vector_destination = nullptr;
    result->output_register = nullptr;
    if (source.output_flag != 0)
    {
        replace_shader_validation_output_destination(*result, source.output_register,
            ShaderValidationChannelMask{0x00203790, source.output_mask.channels});
        source.output_register = nullptr;
        empty.vtable_address = 0x00203790;
        decode_shader_validation_channel_mask(empty, 0);
        copy_shader_validation_channel_mask(source.output_mask, empty);
    }
    if (source.scalar_destination != nullptr)
    {
        replace_shader_validation_scalar_destination(*result, source.scalar_destination,
            ShaderValidationChannelMask{0x00203790, source.scalar_mask.channels});
        source.scalar_destination = nullptr;
        empty.vtable_address = 0x00203790;
        decode_shader_validation_channel_mask(empty, 0);
        copy_shader_validation_channel_mask(source.scalar_mask, empty);
    }
    return result;
}

// Xbox 0x001708D5, 0x001708DF, 0x001708E9 and 0x001708F3: each unwind state frees the
// allocation held in EBP-0x10 when that state's constructor does not complete.
void unwind_instruction_allocation(ShaderValidationConstructorContext& context, void* allocation)
{
    release_shader_validation_allocation(allocation, context.release);
}

ShaderValidationOperand* create_instruction_operand(ShaderValidationConstructorContext& context, ShaderValidationInstruction& instruction,
    u32 slot, void*& allocation)
{
    if (allocation == nullptr)
    {
        return nullptr;
    }
    return construct_shader_validation_operand(context, *static_cast<ShaderValidationOperand*>(allocation),
        context.retail_address(allocation), &instruction, nullptr, slot, context.active_owner, 0);
}

ShaderValidationInstruction* create_instruction_body(ShaderValidationConstructorContext& context, i32& state, void*& allocation)
{
    allocation = context.allocate(sizeof(ShaderValidationInstruction), 0x24870000);
    state = 0;
    ShaderValidationInstruction* instruction = nullptr;
    if (allocation != nullptr)
    {
        instruction = construct_shader_validation_instruction_base(context.objects, *static_cast<ShaderValidationInstruction*>(allocation),
            context.retail_address(allocation));
    }
    instruction->sequence = 0;
    instruction->instruction_set_count = 0;
    instruction->instruction_set = nullptr;
    instruction->vector_opcode = 0;
    instruction->scalar_opcode = 0;
    ShaderValidationChannelMask empty;
    empty.vtable_address = 0x00203790;
    decode_shader_validation_channel_mask(empty, 0);
    copy_shader_validation_channel_mask(instruction->scalar_mask, empty);
    copy_shader_validation_channel_mask(instruction->vector_mask, instruction->scalar_mask);
    copy_shader_validation_channel_mask(instruction->output_mask, instruction->vector_mask);
    instruction->output_flag = 0;
    for (u32 slot = 0; slot < 3; ++slot)
    {
        allocation = context.allocate(sizeof(ShaderValidationOperand), 0x24870000);
        state = static_cast<i32>(slot) + 1;
        instruction->operands[slot] = create_instruction_operand(context, *instruction, slot, allocation);
    }
    instruction->scalar_destination = nullptr;
    instruction->vector_destination = nullptr;
    instruction->output_register = nullptr;
    return instruction;
}
}

// Xbox 0x00192F0C
ShaderValidationInstruction* split_shader_validation_scalar_instruction(ShaderValidationConstructorContext& context,
    ShaderValidationInstruction& source)
{
    i32 state = -1;
    void* first = nullptr;
    void* second = nullptr;
    void* third = nullptr;
    void* fourth = nullptr;
    ShaderValidationInstruction* result = nullptr;
    __try
    {
        result = split_scalar_body(context, source, state, first, second, third, fourth);
    }
    __finally
    {
        if (AbnormalTermination())
        {
            // FuncInfo 0x0032B9BC: states 2->1 and 4->3; all others unwind to -1.
            switch (state)
            {
            case 0:
                release_shader_validation_allocation(first, context.release);
                break;
            case 2:
                release_shader_validation_allocation(third, context.release);
                [[fallthrough]];
            case 1:
            case 6:
                release_shader_validation_allocation(second, context.release);
                break;
            case 4:
                release_shader_validation_allocation(second, context.release);
                [[fallthrough]];
            case 3:
                release_shader_validation_allocation(third, context.release);
                break;
            case 5:
                release_shader_validation_allocation(fourth, context.release);
                break;
            }
        }
    }
    return result;
}

// Xbox 0x00192DF1
ShaderValidationInstruction* create_shader_validation_instruction(ShaderValidationConstructorContext& context)
{
    i32 state = -1;
    void* allocation = nullptr;
    ShaderValidationInstruction* instruction = nullptr;
    __try
    {
        instruction = create_instruction_body(context, state, allocation);
    }
    __finally
    {
        if (AbnormalTermination())
        {
            // Every state in FuncInfo 0x0032B968 unwinds directly to -1.
            if (state >= 0 && state <= 3)
            {
                unwind_instruction_allocation(context, allocation);
            }
        }
    }
    return instruction;
}
}
