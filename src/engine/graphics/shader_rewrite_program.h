#pragma once
#include "gbtg/types.h"

namespace gbtg
{
u32 move_runtime_shader_instruction(u32*& program, i32 destination, i32 source); // Xbox 0x0018689E
u32 replace_runtime_shader_instruction_pair(u32*& program, i32 destination, i32 removed, const u32* replacement); // Xbox 0x00186913
u32 runtime_shader_needs_register_one_rewrite(const u32* program, u8* required); // Xbox 0x001869EC
bool runtime_shader_instruction_has_cost(u8* schedule, const u32* instruction); // Xbox 0x0018C329
}
