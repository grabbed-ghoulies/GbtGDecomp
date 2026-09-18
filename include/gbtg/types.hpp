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

namespace gbtg {

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i16 = std::int16_t;
using i32 = std::int32_t;

class Error : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

inline u32 read_u32(std::span<const u8> b, size_t off) {
    if (off + 4 > b.size()) {
        throw Error("read_u32 out of range");
    }
    return u32(b[off]) | (u32(b[off + 1]) << 8) | (u32(b[off + 2]) << 16) | (u32(b[off + 3]) << 24);
}

inline u16 read_u16(std::span<const u8> b, size_t off) {
    if (off + 2 > b.size()) {
        throw Error("read_u16 out of range");
    }
    return u16(b[off] | (b[off + 1] << 8));
}

inline float read_f32(std::span<const u8> b, size_t off) {
    u32 u = read_u32(b, off);
    float f;
    std::memcpy(&f, &u, 4);
    return f;
}

std::vector<u8> read_file(const std::filesystem::path& path);
void write_file(const std::filesystem::path& path, std::span<const u8> data);
std::string sha256_hex(std::span<const u8> data);
std::string sha256_file(const std::filesystem::path& path);
std::vector<u8> zlib_inflate(std::span<const u8> compressed, size_t expected = 0);

}  // namespace gbtg
