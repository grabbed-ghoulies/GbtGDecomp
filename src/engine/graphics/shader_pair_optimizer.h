#pragma once
#include "gbtg/types.h"

namespace gbtg
{
void decode_runtime_shader_write_registers(u8* output, const u32* instruction); // Xbox 0x00187E9B
bool runtime_shader_has_constant_input(const u32* instruction); // Xbox 0x0018735A
bool runtime_shader_constant_inputs_compatible(const u32* first, const u32* second); // Xbox 0x001873C8
bool runtime_shader_swizzle_is_identity(u8 mask, i32 negate, i32 x, i32 y, i32 z, i32 w); // Xbox 0x0018731B
bool runtime_shader_instruction_dependency(const u32* first, const u32* second); // Xbox 0x0018A506
bool combine_runtime_shader_parallel_instructions(u32* output, const u32* first, const u32* second); // Xbox 0x0018A8DD
bool combine_runtime_shader_move_instruction(u32* output, const u32* first, const u32* second); // Xbox 0x00187406
bool combine_runtime_shader_instruction_pair(u32* output, const u32* first, const u32* second); // Xbox 0x0018ACFB
u32 compact_runtime_shader_instruction_pairs(u32* program); // Xbox 0x0018AD2D
}
