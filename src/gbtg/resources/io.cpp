#include "gbtg/types.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <zlib.h>

namespace gbtg
{
namespace
{

constexpr u32 kShaK[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

u32 rotr(u32 x, u32 n)
{
    return (x >> n) | (x << (32 - n));
}

void sha256_transform(u32 s[8], const u8 block[64])
{
    u32 w[64];
    for (int i = 0; i < 16; ++i)
    {
        w[i] = (u32(block[i * 4]) << 24) | (u32(block[i * 4 + 1]) << 16) | (u32(block[i * 4 + 2]) << 8) |
               u32(block[i * 4 + 3]);
    }
    for (int i = 16; i < 64; ++i)
    {
        const u32 s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        const u32 s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    u32 a = s[0], b = s[1], c = s[2], d = s[3], e = s[4], f = s[5], g = s[6], h = s[7];
    for (int i = 0; i < 64; ++i)
    {
        const u32 S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        const u32 ch = (e & f) ^ ((~e) & g);
        const u32 t1 = h + S1 + ch + kShaK[i] + w[i];
        const u32 S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        const u32 maj = (a & b) ^ (a & c) ^ (b & c);
        const u32 t2 = S0 + maj;
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    s[0] += a;
    s[1] += b;
    s[2] += c;
    s[3] += d;
    s[4] += e;
    s[5] += f;
    s[6] += g;
    s[7] += h;
}

} // namespace

std::vector<u8> read_file(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        throw Error("cannot open " + path.string());
    }
    in.seekg(0, std::ios::end);
    const auto n = static_cast<size_t>(in.tellg());
    in.seekg(0);
    std::vector<u8> b(n);
    if (n && !in.read(reinterpret_cast<char*>(b.data()), static_cast<std::streamsize>(n)))
    {
        throw Error("short read " + path.string());
    }
    return b;
}

void write_file(const std::filesystem::path& path, std::span<const u8> data)
{
    if (!path.parent_path().empty())
    {
        std::filesystem::create_directories(path.parent_path());
    }
    std::ofstream out(path, std::ios::binary);
    if (!out)
    {
        throw Error("cannot write " + path.string());
    }
    out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
}

std::string sha256_hex(std::span<const u8> data)
{
    u32 s[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    u8 block[64];
    size_t i = 0;
    for (; i + 64 <= data.size(); i += 64)
    {
        sha256_transform(s, data.data() + i);
    }
    size_t rem = data.size() - i;
    std::memset(block, 0, 64);
    if (rem)
    {
        std::memcpy(block, data.data() + i, rem);
    }
    block[rem] = 0x80;
    if (rem >= 56)
    {
        sha256_transform(s, block);
        std::memset(block, 0, 64);
    }
    const u64 bits = u64(data.size()) * 8;
    for (int b = 0; b < 8; ++b)
    {
        block[63 - b] = static_cast<u8>(bits >> (b * 8));
    }
    sha256_transform(s, block);
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (int k = 0; k < 8; ++k)
    {
        oss << std::setw(8) << s[k];
    }
    return oss.str();
}

std::string sha256_file(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        throw Error("cannot open " + path.string());
    }
    u32 s[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    u8 buf[64 * 1024];
    u8 block[64];
    u64 total = 0;
    size_t leftover = 0;
    u8 leftover_buf[64];
    while (in)
    {
        in.read(reinterpret_cast<char*>(buf), sizeof(buf));
        const auto n = static_cast<size_t>(in.gcount());
        if (!n)
        {
            break;
        }
        total += n;
        size_t off = 0;
        if (leftover)
        {
            const size_t need = 64 - leftover;
            const size_t take = std::min(need, n);
            std::memcpy(leftover_buf + leftover, buf, take);
            leftover += take;
            off = take;
            if (leftover == 64)
            {
                sha256_transform(s, leftover_buf);
                leftover = 0;
            }
        }
        for (; off + 64 <= n; off += 64)
        {
            sha256_transform(s, buf + off);
        }
        if (off < n)
        {
            leftover = n - off;
            std::memcpy(leftover_buf, buf + off, leftover);
        }
    }
    std::memset(block, 0, 64);
    if (leftover)
    {
        std::memcpy(block, leftover_buf, leftover);
    }
    block[leftover] = 0x80;
    if (leftover >= 56)
    {
        sha256_transform(s, block);
        std::memset(block, 0, 64);
    }
    const u64 bits = total * 8;
    for (int b = 0; b < 8; ++b)
    {
        block[63 - b] = static_cast<u8>(bits >> (b * 8));
    }
    sha256_transform(s, block);
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (int k = 0; k < 8; ++k)
    {
        oss << std::setw(8) << s[k];
    }
    return oss.str();
}

std::vector<u8> zlib_inflate(std::span<const u8> compressed, size_t expected)
{
    if (compressed.empty())
    {
        return {};
    }
    z_stream strm{};
    if (inflateInit(&strm) != Z_OK)
    {
        throw Error("inflateInit failed");
    }
    strm.next_in = const_cast<Bytef*>(compressed.data());
    strm.avail_in = static_cast<uInt>(compressed.size());
    std::vector<u8> out;
    out.resize(expected ? expected : compressed.size() * 4);
    int ret = Z_OK;
    while (ret == Z_OK)
    {
        if (strm.total_out >= out.size())
        {
            out.resize(out.size() * 2 + 1024);
        }
        strm.next_out = out.data() + strm.total_out;
        strm.avail_out = static_cast<uInt>(out.size() - strm.total_out);
        ret = inflate(&strm, Z_NO_FLUSH);
    }
    const size_t produced = strm.total_out;
    inflateEnd(&strm);
    if (ret != Z_STREAM_END)
    {
        throw Error("zlib inflate failed");
    }
    out.resize(produced);
    if (expected && produced != expected)
    {
        throw Error("zlib size mismatch");
    }
    return out;
}

std::vector<u8> zlib_deflate(std::span<const u8> raw)
{
    if (raw.empty())
    {
        return {};
    }
    z_stream strm{};
    if (deflateInit(&strm, Z_DEFAULT_COMPRESSION) != Z_OK)
    {
        throw Error("deflateInit failed");
    }
    strm.next_in = const_cast<Bytef*>(raw.data());
    strm.avail_in = static_cast<uInt>(raw.size());
    std::vector<u8> out(raw.size() + 64);
    int ret = Z_OK;
    while (ret == Z_OK)
    {
        if (strm.total_out >= out.size())
        {
            out.resize(out.size() * 2 + 1024);
        }
        strm.next_out = out.data() + strm.total_out;
        strm.avail_out = static_cast<uInt>(out.size() - strm.total_out);
        ret = deflate(&strm, Z_FINISH);
    }
    const size_t produced = strm.total_out;
    deflateEnd(&strm);
    if (ret != Z_STREAM_END)
    {
        throw Error("zlib deflate failed");
    }
    out.resize(produced);
    return out;
}

} // namespace gbtg
