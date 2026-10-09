#pragma once
#include "gbtg/types.h"

namespace gbtg
{
long double runtime_shader_first_operand_cost(u8* state, const u32* instruction, u32 mask, float issued, u32& reason); // Xbox 0x0018B9C9
long double runtime_shader_second_operand_cost(u8* state, const u32* instruction, u32 mask, float issued, u32& reason); // Xbox 0x0018BA2D
long double runtime_shader_third_operand_cost(u8* state, const u32* instruction, u32 mask, float issued, u32& reason); // Xbox 0x0018BA95
bool runtime_shader_first_operand_ready(u8* state, const u32* instruction, u32 mask, i32 issued); // Xbox 0x0018BE49
bool runtime_shader_second_operand_ready(u8* state, const u32* instruction, u32 mask, i32 issued); // Xbox 0x0018BEA8
bool runtime_shader_third_operand_ready(u8* state, const u32* instruction, u32 mask, i32 issued); // Xbox 0x0018BF0B
long double runtime_shader_instruction_cost(u8* state, const u32* instruction, u32& reason); // Xbox 0x0018BF95
u32 issue_runtime_shader_instruction(u8* state, const u32* instruction, float* cost, u32* reason); // Xbox 0x0018C0DB
u32 estimate_runtime_shader_program_cost(u8* state, u8 mode, const u32* program, float* cost); // Xbox 0x0018C277
u32 optimize_runtime_shader_multiply_order(u8* state, u8 mode, u32* program); // Xbox 0x0018C1B9
}
