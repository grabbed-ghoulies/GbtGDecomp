#pragma once
#include "shader_merge_pass.h"
namespace gbtg
{
bool runtime_shader_reorder_is_safe(u32*& program, const u8* writes, i32 first, i32 candidate); // Xbox 0x0018B403
bool find_runtime_shader_merge_candidate(u32*& program, i32 first, i32* candidate, u32* output,
    RuntimeShaderMergeScratch& scratch); // Xbox 0x0018B47B
bool find_runtime_shader_ready_candidate(u32*& program, u8* schedule, i32 first, i32* candidate); // Xbox 0x0018C358
u32 reorder_runtime_shader_program(u32*& program, u8* schedule, u8 mode,
    RuntimeShaderMergeScratch& scratch); // Xbox 0x0018C3F6
}
