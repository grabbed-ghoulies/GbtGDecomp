#pragma once
#include "gbtg/types.h"

namespace gbtg
{
u32 remap_runtime_shader_rewrite_mask(u8* output, u8 mask, u32 x, u32 y, u32 z, u32 w); // Xbox 0x001864C4
u32 decode_runtime_shader_rewrite_source(u8 mask, u8* sources, u8* output, const u32* instruction, u8 index, u16 type); // Xbox 0x00186520
i32 filter_runtime_shader_rewrite_destination(u8* storage, const u32* instruction, u8* destination); // Xbox 0x0018657E
i32 retain_runtime_shader_rewrite_destination(u8* storage, const u32* instruction, const u8* destination); // Xbox 0x001865DB
i32 retain_runtime_shader_rewrite_source(u8* storage, const u32* instruction, const u8* source, const void* retained); // Xbox 0x00186617
i32 filter_runtime_shader_rewrite_destinations(u8* storage, const u32* instruction, u8* destinations); // Xbox 0x00189ADC
i32 retain_runtime_shader_rewrite_destinations(u8* storage, const u32* instruction, const u8* destinations); // Xbox 0x00189B3F
i32 retain_runtime_shader_rewrite_sources(u8* storage, const u32* instruction, const u8* sources); // Xbox 0x00189BA2
i32 decode_runtime_shader_rewrite_sources(const u32* instruction, const u8* destinations, u8* sources); // Xbox 0x00189858
i32 rewrite_runtime_shader_instruction(u8* storage, const u32* instruction, u32*& output); // Xbox 0x0018B299
u32 remove_runtime_shader_dead_writes(u8* storage, u32 options, u32* program); // Xbox 0x0018BB40
}
