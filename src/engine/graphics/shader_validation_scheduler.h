#pragma once
#include "shader_validation_packed_program.h"
#include "shader_validation_instruction_lifecycle.h"

namespace gbtg
{
struct ShaderValidationScheduleContext
{
    ShaderValidationProgramConstructorContext& programs;
    ShaderValidationInstructionDestroyContext& instructions;
    u8& conservative; // DAT_001a4d31.
    ShaderValidationSerializerFrame& serializer_frame;
};
bool schedule_shader_validation_program(ShaderValidationScheduleContext& context,
    ShaderValidationProgramPass& pass); // Xbox 0x001973F1
bool schedule_and_serialize_shader_validation_program(ShaderValidationScheduleContext& context,
    ShaderValidationProgramPass& pass, u32* packed_program); // Xbox 0x00197A94
}
