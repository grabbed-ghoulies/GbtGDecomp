#pragma once
#include "gbtg/types.h"

namespace gbtg
{
inline constexpr u32 runtime_shader_rewrite_storage_size = 0x208;
u32 initialize_runtime_shader_rewrite_storage(u8* storage, u32 options); // Xbox 0x00186679
u32 decode_runtime_shader_rewrite_destinations(const u32* instruction, u8* destinations); // Xbox 0x001862A8
u32 apply_runtime_shader_rewrite_destinations(u32* instruction, const u8* destinations); // Xbox 0x00186394
}
