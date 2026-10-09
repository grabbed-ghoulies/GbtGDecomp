#pragma once
#include "gbtg/types.h"

namespace gbtg
{
void remove_runtime_shader_vector_instruction(u32* instruction); // Xbox 0x001872C1
long double runtime_shader_register_hazard_cost(const u8* table, u8 reg, u32 channel, u8 scalar, u32& reason); // Xbox 0x00185F25
long double runtime_shader_masked_hazard_cost(const u8* state, const u32* instruction, u8 operand, u32 reg, u8 mask, float issued, u32& reason); // Xbox 0x00189723
bool runtime_shader_register_bypass_ready(const u8* table, const u32* instruction, i32 operand, u8 reg, u8 mask, i32 issued); // Xbox 0x001897A3
bool runtime_shader_masked_hazard_ready(const u8* state, const u32* instruction, u8 operand, u32 reg, u8 mask, i32 issued); // Xbox 0x0018B243
long double runtime_shader_operand_hazard_cost(u8* state, const u32* instruction, i32 type, u32 reg, u32 negate, u32 w, u32 z, u32 y, u32 x, u32 operand, u32 mask, float issued, u32& reason); // Xbox 0x0018B1EE
bool runtime_shader_operand_hazard_ready(u8* state, const u32* instruction, i32 type, u32 reg, u32 negate, u32 w, u32 z, u32 y, u32 x, u32 operand, u32 mask, i32 issued); // Xbox 0x0018BAF8
}
