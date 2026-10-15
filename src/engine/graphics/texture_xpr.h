#pragma once
#include "gbtg/types.h"

#include <span>

namespace gbtg
{
// The bytes a D3D texture header (0x14 bytes: Common, Data, Lock, Format, Size) describes: a linear surface's
// pitch times height, or every swizzled level down to 1x1 (DXT down to a 4x4 block).
u32 xbox_texture_data_size(std::span<const u8> header);
// XGWriteSurfaceOrTextureToXPR (0x001524B9) as host file IO: "XPR0", the total and header sizes, the texture
// header with its data offset 0, the 0xFFFFFFFF end mark, 0xAD padding to 0x800, then the texture data.
void write_texture_xpr(std::span<const u8> header, std::span<const u8> data, const char* path);
}
