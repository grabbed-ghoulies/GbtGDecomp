#pragma once
#include "gbtg/types.h"

namespace gbtg
{
bool runtime_shader_vector_register_write_is_identity(const u32* instruction); // Xbox 0x00185F5F
bool runtime_shader_vector_constant_write_is_identity(const u32* instruction); // Xbox 0x00185FDF
bool runtime_shader_scalar_register_write_is_identity(const u32* instruction); // Xbox 0x0018607B
bool runtime_shader_scalar_constant_write_is_identity(const u32* instruction); // Xbox 0x00186120
u32 remove_runtime_shader_identity_writes(u32* instruction); // Xbox 0x001861BB
bool runtime_shader_instruction_outputs_are_redundant(const u32* instruction); // Xbox 0x00186201
}
