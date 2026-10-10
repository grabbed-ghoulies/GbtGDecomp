#pragma once
#include "shader_validation_constructors.h"
#include "shader_validation_containers.h"

namespace gbtg
{
bool clear_unused_shader_validation_operand_channels(ShaderValidationConstructorContext& context,
    ShaderValidationOperand& operand); // Xbox 0x0019373D
bool clear_unused_shader_validation_instruction_channels(ShaderValidationConstructorContext& context,
    ShaderValidationInstruction& instruction); // Xbox 0x0019379C

// Allocates and constructs a validator instruction with three empty operands.
// Retail dereferences the instruction allocation without a null test.
ShaderValidationInstruction* create_shader_validation_instruction(ShaderValidationConstructorContext& context); // Xbox 0x00192DF1
ShaderValidationInstruction* split_shader_validation_scalar_instruction(ShaderValidationConstructorContext& context,
    ShaderValidationInstruction& source); // Xbox 0x00192F0C
}
