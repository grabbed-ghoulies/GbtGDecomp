#pragma once
#include "gbtg/types.h"

namespace gbtg
{
u32 record_runtime_shader_register_writes(u8* state, const u32* instruction); // Xbox 0x00189499
u32 record_runtime_shader_issue_writes(u8* state, const u32* instruction, i32 reason); // Xbox 0x001895E1
}
