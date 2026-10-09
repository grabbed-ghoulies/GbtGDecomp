#pragma once
#include "gbtg/types.h"

namespace gbtg
{
bool combine_runtime_shader_multiply_add(u32* output, const u32* first, const u32* second); // Xbox 0x00187B22
bool convert_runtime_shader_move_to_scalar(u32* output, const u32* input); // Xbox 0x00187057
bool swap_runtime_shader_multiply_operands(u32* output, const u32* input); // Xbox 0x001871AD
bool equal_runtime_shader_operand(u32 type, u32 index, u32 negate, const u32* first,
    u32 other_type, u32 other_index, u32 other_negate, const u32* second); // Xbox 0x00187522
u8 runtime_shader_uses_first_operand(const u32* instruction); // Xbox 0x00187E46
u8 runtime_shader_uses_second_operand(const u32* instruction); // Xbox 0x00187E5C
bool runtime_shader_uses_third_operand(const u32* instruction); // Xbox 0x00187E72
bool runtime_shader_vector_uses_type(const u32* instruction, u32 type); // Xbox 0x00187A59
bool runtime_shader_scalar_uses_type(const u32* instruction, u32 type); // Xbox 0x00187AB8
bool runtime_shader_uses_type(const u32* instruction, u32 type); // Xbox 0x00187ADA
bool runtime_shader_uses_constant(const u32* instruction); // Xbox 0x00187B06
bool runtime_shader_uses_vertex_input(const u32* instruction); // Xbox 0x00187B14
bool pack_runtime_shader_destinations(u32* instruction, const u8* destinations); // Xbox 0x00187F19
bool merge_runtime_shader_destinations(u32* output, const u32* first, const u32* second); // Xbox 0x00187F88
bool merge_runtime_shader_instruction(u32* output, const u32* first, const u32* second, u32* error); // Xbox 0x00188032
}
