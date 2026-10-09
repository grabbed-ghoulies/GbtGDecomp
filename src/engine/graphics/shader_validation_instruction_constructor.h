#pragma once
#include "shader_validation_constructors.h"
#include "shader_validation_containers.h"

namespace gbtg
{
// Allocates and constructs a validator instruction with three empty operands.
// Retail dereferences the instruction allocation without a null test.
ShaderValidationInstruction* create_shader_validation_instruction(ShaderValidationConstructorContext& context); // Xbox 0x00192DF1
}
