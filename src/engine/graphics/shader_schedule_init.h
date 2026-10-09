#pragma once
#include "gbtg/types.h"

namespace gbtg
{
// Borrowed retail layout; callers retain bytes not written by initialization.
inline constexpr u32 runtime_shader_schedule_storage_size = 0x8C4;
void initialize_runtime_shader_register_schedule(u8* registers); // Xbox 0x00185807
void initialize_runtime_shader_issue_schedule(u8* issue, u32 option); // Xbox 0x0018586A
u32 initialize_runtime_shader_schedule(u8* state, u32 option); // Xbox 0x00189462
u32 configure_runtime_shader_schedule(u8* state, u8 mode, u32 option); // Xbox 0x0018B1D3
}
