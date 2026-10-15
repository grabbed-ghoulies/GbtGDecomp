#include "texture_xpr.h"
#include "../saves/save_root.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <vector>

namespace gbtg
{
namespace
{
u32 word(std::span<const u8> bytes, size_t offset)
{
    u32 value;
    std::memcpy(&value, bytes.data() + offset, 4);
    return value;
}

// Bits per texel of an Xbox D3DFORMAT, or 0 for the compressed formats (handled by blocks).
u32 texel_bits(u32 format)
{
    switch (format)
    {
    case 0x00: // L8
    case 0x01: // AL8
    case 0x0B: // P8
    case 0x13: // LIN_L8
        return 8;
    case 0x02:
    case 0x03:
    case 0x04:
    case 0x05:
    case 0x10:
    case 0x11:
        return 16;
    case 0x06:
    case 0x07:
    case 0x12:
    case 0x1E:
    case 0x3A:
    case 0x3F:
        return 32;
    default:
        throw Error("texture XPR: format " + std::to_string(format) + " has no size rule");
    }
}
}

u32 xbox_texture_data_size(std::span<const u8> header)
{
    const u32 format_word = word(header, 0xC);
    const u32 size_word = word(header, 0x10);
    const u32 format = (format_word >> 8) & 0xFF;
    if (size_word != 0)
    {
        const u32 height = ((size_word >> 12) & 0xFFF) + 1;
        const u32 pitch = ((size_word >> 24) + 1) * 64;
        return pitch * height;
    }
    const u32 levels = std::max(1u, (format_word >> 16) & 0xF);
    u32 width = 1u << ((format_word >> 20) & 0xF);
    u32 height = 1u << ((format_word >> 24) & 0xF);
    u32 total = 0;
    for (u32 level = 0; level < levels; ++level)
    {
        if (format == 0x0C || format == 0x0E || format == 0x0F) // DXT1, DXT3, DXT5
        {
            const u32 blocks = std::max(1u, width / 4) * std::max(1u, height / 4);
            total += blocks * (format == 0x0C ? 8u : 16u);
        }
        else
        {
            total += width * height * texel_bits(format) / 8;
        }
        width = std::max(1u, width / 2);
        height = std::max(1u, height / 2);
    }
    return total;
}

void write_texture_xpr(std::span<const u8> header, std::span<const u8> data, const char* path)
{
    constexpr u32 kHeaderSize = 0x800;
    std::vector<u8> bytes(kHeaderSize, 0xAD);
    const u32 total = kHeaderSize + u32(data.size());
    std::memcpy(bytes.data(), "XPR0", 4);
    std::memcpy(bytes.data() + 4, &total, 4);
    std::memcpy(bytes.data() + 8, &kHeaderSize, 4);
    std::memcpy(bytes.data() + 0xC, header.data(), 0x14);
    const u32 data_offset = 0;
    std::memcpy(bytes.data() + 0xC + 4, &data_offset, 4);
    const u32 end = 0xFFFFFFFFu;
    std::memcpy(bytes.data() + 0xC + 0x14, &end, 4);
    bytes.insert(bytes.end(), data.begin(), data.end());
    std::ofstream file(xbox_host_path(path), std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
}
}
