#pragma once
#include "gbtg/types.h"

namespace gbtg
{
void decode_runtime_shader_operand_masks(const u32* instruction, u8* masks); // Xbox 0x00187584
void decode_runtime_shader_masked_swizzle(u8 mask, u8 w, u8 z, u8 y, u8 x, u8* output); // Xbox 0x00187621
void decode_runtime_shader_swizzles(const u32* instruction, u8* output); // Xbox 0x00187671
bool runtime_shader_swizzle_has_default_channels(const u8* channels); // Xbox 0x001877CE
u8 runtime_shader_swizzle_fallback(const u8* channels); // Xbox 0x0018780B
i32 resolve_runtime_shader_swizzle_channel(u8 channel, u8 fallback, u8 default_channel); // Xbox 0x00187833
void encode_runtime_shader_swizzles(u32* instruction, const u8* channels); // Xbox 0x0018784D
bool merge_runtime_shader_swizzles(u32* output, const u32* first, const u32* second); // Xbox 0x00187A01
}
