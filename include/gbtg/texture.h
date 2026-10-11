#pragma once

#include "gbtg/types.h"

namespace gbtg
{

enum class XboxFormat : u32
{
    L8 = 0x00,
    AL8 = 0x01,
    A1R5G5B5 = 0x02,
    X1R5G5B5 = 0x03,
    A4R4G4B4 = 0x04,
    R5G6B5 = 0x05,
    A8R8G8B8 = 0x06,
    X8R8G8B8 = 0x07,
    P8 = 0x0B,
    DXT1 = 0x0C,
    DXT3 = 0x0E,
    DXT5 = 0x0F,
    LinA1R5G5B5 = 0x10,
    LinR5G6B5 = 0x11,
    LinA8R8G8B8 = 0x12,
    LinL8 = 0x13,
    LinX8R8G8B8 = 0x1E,
};

struct TextureHeader
{
    XboxFormat format{};
    u32 header_size = 0;
    u16 width = 0;
    u16 height = 0;
    u32 flags = 0;
    u8 unknown = 0;
    u8 tile_count = 0;
    u16 pad = 0;
    u32 texture_offset = 0;
    u32 data_size = 0;
};

struct RgbaImage
{
    int width = 0;
    int height = 0;
    std::vector<u8> bgra; // 4 bytes per pixel, BGRA
    u32 serial = 0;       // bumps when the pixels change, so a cached upload is replaced
    // Texture header flags >> 16. Nonzero marks the surface 0x000527C0 registers for BinkCopyToBuffer.
    u8 movie_slot = 0;
    u32 movie_seen = 0;
    bool movie_front = false;   // draw this surface after a coplanar still that covers it
    std::vector<u8> movie_base; // pixels before a movie replaced them
};

bool parse_texture_header(std::span<const u8> cpu, TextureHeader& out);
RgbaImage decode_xbox_texture(const TextureHeader& hdr, std::span<const u8> gpu);
void write_bmp(const std::filesystem::path& path, const RgbaImage& img);
void write_png(const std::filesystem::path& path, const RgbaImage& img);
const char* xbox_format_name(XboxFormat f);

} // namespace gbtg
