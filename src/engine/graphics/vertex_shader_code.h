#pragma once
#include "gbtg/types.h"

namespace gbtg
{
u8 runtime_vertex_shader_code_type(const u16* code); // Xbox 0x0018850C
void unpack_runtime_vertex_instruction(const u32* input, u32* output); // Xbox 0x00186FD5
}
