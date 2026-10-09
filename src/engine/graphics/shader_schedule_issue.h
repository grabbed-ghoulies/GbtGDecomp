#pragma once
#include "gbtg/types.h"

namespace gbtg
{
void advance_runtime_shader_issue_schedule(u8* table); // Xbox 0x001858E5
u32 runtime_shader_vector_latency(const u32* instruction); // Xbox 0x001857E5
void write_runtime_shader_register_schedule(u8* table, u32 latency, u8 reg, u8 mask, u32 issued); // Xbox 0x0018582C
u32 runtime_shader_swizzle_channel_mask(u8 w, u8 z, u8 y, u8 x); // Xbox 0x00187718
u8 runtime_shader_operand_channel_mask(const u32* instruction, u32 operand); // Xbox 0x00187760
}
