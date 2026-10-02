#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <span>
#include <optional>
#include <filesystem>
#include <stdexcept>
#include <array>
#include <cstring>

namespace gbtg
{

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i16 = std::int16_t;
using i32 = std::int32_t;

class Error : public std::runtime_error
{
  public:
    using std::runtime_error::runtime_error;
};

inline u32 read_u32(std::span<const u8> b, size_t off)
{
    if (off + 4 > b.size())
    {
        throw Error("read_u32 out of range");
    }
    return u32(b[off]) | (u32(b[off + 1]) << 8) | (u32(b[off + 2]) << 16) | (u32(b[off + 3]) << 24);
}

inline u16 read_u16(std::span<const u8> b, size_t off)
{
    if (off + 2 > b.size())
    {
        throw Error("read_u16 out of range");
    }
    return u16(b[off] | (b[off + 1] << 8));
}

inline float read_f32(std::span<const u8> b, size_t off)
{
    u32 u = read_u32(b, off);
    float f;
    std::memcpy(&f, &u, 4);
    return f;
}

inline void write_u32(std::vector<u8>& b, size_t off, u32 v)
{
    if (off + 4 > b.size())
    {
        throw Error("write_u32 out of range");
    }
    b[off] = u8(v);
    b[off + 1] = u8(v >> 8);
    b[off + 2] = u8(v >> 16);
    b[off + 3] = u8(v >> 24);
}

inline void write_u16(std::vector<u8>& b, size_t off, u16 v)
{
    if (off + 2 > b.size())
    {
        throw Error("write_u16 out of range");
    }
    b[off] = u8(v);
    b[off + 1] = u8(v >> 8);
}

struct Vec3
{
    float x = 0, y = 0, z = 0;
};

// Cutscene camera orientation. Channels 3..6 are x, y, z, w (0x0004F8C0 / 0x00051590).
struct Quat
{
    float x = 0, y = 0, z = 0, w = 1;
};

struct Vec2
{
    float u = 0, v = 0;
};

std::vector<u8> read_file(const std::filesystem::path& path);
void write_file(const std::filesystem::path& path, std::span<const u8> data);
std::string sha256_hex(std::span<const u8> data);
std::string sha256_file(const std::filesystem::path& path);
std::vector<u8> zlib_inflate(std::span<const u8> compressed, size_t expected = 0);
std::vector<u8> zlib_deflate(std::span<const u8> raw);

} // namespace gbtg
