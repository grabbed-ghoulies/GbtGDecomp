#pragma once
#include "shader_validation_optimizer.h"
#include "shader_validation_scheduler.h"

namespace gbtg
{
struct ShaderValidationProgramContext
{
    ShaderValidationOptimizationContext& optimization;
    ShaderValidationScheduleContext& schedule;
    ShaderValidationPassDestroyContext& passes;
    u8& split_scalar; // DAT_001a4d32.
    u8& assigning_registers; // DAT_001a4d33.
    // Genuine registered-object vtable slot +0x00. The address map dispatches
    // to the translated deleting destructor for operands, instructions, owners,
    // passes and registered pass-tree objects.
    std::function<void(u32, u8)> delete_registered_object;
};
bool validate_shader_validation_program(ShaderValidationProgramContext& context,
    u32* packed_program); // Xbox 0x0019723C
// The same registry operations with balanced nodes storing mapped retail addresses.
void reset_shader_validation_tree_storage(ShaderValidationBalancedTree& objects); // Xbox 0x00190C93
void release_shader_validation_storage(ShaderValidationProgramContext& context); // Xbox 0x00190CC1
}
