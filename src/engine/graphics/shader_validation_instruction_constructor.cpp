#include "shader_validation_instruction_constructor.h"
#include "shader_validation_cleanup.h"
#if defined(_WIN32)
#include <excpt.h>
#else
#error Shader validator constructor unwind requires the approved Windows SEH backend.
#endif

namespace gbtg
{
namespace
{
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
