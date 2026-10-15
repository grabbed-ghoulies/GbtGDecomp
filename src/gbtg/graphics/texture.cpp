#include "gbtg/texture.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <zlib.h>

namespace gbtg
{
namespace
{

// Original Xbox NV2A Morton (Z-order) swizzle, not Xbox 360 XG tiling.
std::vector<u8> unswizzle_nv2a(std::span<const u8> src, u32 w, u32 h, u32 bpp)
{
    u32 mask_x = 0, mask_y = 0, bit = 1, mask_bit = 1;
    for (;;)
    {
        bool done = true;
        if (bit < w)
        {
            mask_x |= mask_bit;
            mask_bit <<= 1;
            done = false;
        }
        if (bit < h)
        {
            mask_y |= mask_bit;
            mask_bit <<= 1;
            done = false;
        }
        bit <<= 1;
        if (done)
        {
            break;
        }
    }
    std::vector<u8> dst(size_t(w) * h * bpp);
    u32 off_y = 0;
    for (u32 y = 0; y < h; ++y)
    {
        u32 off_x = 0;
        for (u32 x = 0; x < w; ++x)
        {
            const u32 so = (off_y + off_x) * bpp;
            const u32 doff = (y * w + x) * bpp;
            if (so + bpp <= src.size() && doff + bpp <= dst.size())
            {
                std::memcpy(dst.data() + doff, src.data() + so, bpp);
            }
            off_x = (off_x - mask_x) & mask_x;
        }
        off_y = (off_y - mask_y) & mask_y;
    }
    return dst;
}

void decode_rgb565(u16 c, u8& r, u8& g, u8& b)
{
    r = u8(((c >> 11) & 31) * 255 / 31);
    g = u8(((c >> 5) & 63) * 255 / 63);
    b = u8((c & 31) * 255 / 31);
}

void decode_dxt1_block(const u8 block[8], u8 out[16 * 4], bool dxt1_alpha)
{
    const u16 c0 = u16(block[0] | (block[1] << 8));
    const u16 c1 = u16(block[2] | (block[3] << 8));
    u8 r[4], g[4], b[4], a[4];
    decode_rgb565(c0, r[0], g[0], b[0]);
    decode_rgb565(c1, r[1], g[1], b[1]);
    a[0] = a[1] = a[2] = a[3] = 255;
    if (c0 > c1 || !dxt1_alpha)
    {
        r[2] = u8((2 * r[0] + r[1]) / 3);
        g[2] = u8((2 * g[0] + g[1]) / 3);
        b[2] = u8((2 * b[0] + b[1]) / 3);
        r[3] = u8((r[0] + 2 * r[1]) / 3);
        g[3] = u8((g[0] + 2 * g[1]) / 3);
        b[3] = u8((b[0] + 2 * b[1]) / 3);
    }
    else
    {
        r[2] = u8((r[0] + r[1]) / 2);
        g[2] = u8((g[0] + g[1]) / 2);
        b[2] = u8((b[0] + b[1]) / 2);
        r[3] = g[3] = b[3] = 0;
        a[3] = 0;
    }
    u32 idx = u32(block[4]) | (u32(block[5]) << 8) | (u32(block[6]) << 16) | (u32(block[7]) << 24);
    for (int i = 0; i < 16; ++i)
    {
        const u32 sel = (idx >> (i * 2)) & 3;
        out[i * 4 + 0] = b[sel];
        out[i * 4 + 1] = g[sel];
        out[i * 4 + 2] = r[sel];
        out[i * 4 + 3] = a[sel];
    }
}

void decode_dxt3_alpha(const u8 block[8], u8 alpha[16])
{
    for (int i = 0; i < 16; ++i)
    {
        const u8 n = (block[i / 2] >> ((i & 1) * 4)) & 0xF;
        alpha[i] = u8(n * 17);
    }
}

void decode_dxt5_alpha(const u8 block[8], u8 alpha[16])
{
    u8 a[8];
    a[0] = block[0];
    a[1] = block[1];
    if (a[0] > a[1])
    {
        for (int i = 1; i <= 6; ++i)
        {
            a[i + 1] = u8(((7 - i) * a[0] + i * a[1]) / 7);
        }
    }
    else
    {
        for (int i = 1; i <= 4; ++i)
        {
            a[i + 1] = u8(((5 - i) * a[0] + i * a[1]) / 5);
        }
        a[6] = 0;
        a[7] = 255;
    }
    u64 bits = 0;
    for (int i = 0; i < 6; ++i)
    {
        bits |= u64(block[2 + i]) << (8 * i);
    }
    for (int i = 0; i < 16; ++i)
    {
        alpha[i] = a[(bits >> (i * 3)) & 7];
    }
}

RgbaImage decode_dxt(std::span<const u8> src, int w, int h, XboxFormat fmt, bool swizzled)
{
    const int bw = (w + 3) / 4;
    const int bh = (h + 3) / 4;
    const bool is_dxt1 = fmt == XboxFormat::DXT1;
    const int block_bytes = is_dxt1 ? 8 : 16;
    std::vector<u8> linear;
    const u8* blocks = src.data();
    size_t src_size = src.size();
    if (swizzled)
    {
        linear = unswizzle_nv2a(src, u32(bw), u32(bh), u32(block_bytes));
        blocks = linear.data();
        src_size = linear.size();
    }
    RgbaImage img;
    img.width = w;
    img.height = h;
    img.bgra.assign(size_t(w) * h * 4, 0);
    for (int by = 0; by < bh; ++by)
    {
        for (int bx = 0; bx < bw; ++bx)
        {
            const size_t off = size_t(by * bw + bx) * block_bytes;
            if (off + block_bytes > src_size)
            {
                continue;
            }
            u8 color[16 * 4];
            u8 alpha[16];
            std::memset(alpha, 255, 16);
            if (is_dxt1)
            {
                decode_dxt1_block(blocks + off, color, true);
            }
            else if (fmt == XboxFormat::DXT3)
            {
                decode_dxt3_alpha(blocks + off, alpha);
                decode_dxt1_block(blocks + off + 8, color, false);
            }
            else
            {
                decode_dxt5_alpha(blocks + off, alpha);
                decode_dxt1_block(blocks + off + 8, color, false);
            }
            for (int py = 0; py < 4; ++py)
            {
                for (int px = 0; px < 4; ++px)
                {
                    const int x = bx * 4 + px;
                    const int y = by * 4 + py;
                    if (x >= w || y >= h)
                    {
                        continue;
                    }
                    const int si = py * 4 + px;
                    u8* d = img.bgra.data() + (size_t(y) * w + x) * 4;
                    d[0] = color[si * 4 + 0];
                    d[1] = color[si * 4 + 1];
                    d[2] = color[si * 4 + 2];
                    d[3] = (fmt == XboxFormat::DXT1) ? color[si * 4 + 3] : alpha[si];
                }
            }
        }
    }
    return img;
}

RgbaImage decode_argb8888(std::span<const u8> src, int w, int h, bool swizzled)
{
    std::vector<u8> linear;
    std::span<const u8> pix = src;
    if (swizzled)
    {
        linear = unswizzle_nv2a(src, u32(w), u32(h), 4);
        pix = linear;
    }
    RgbaImage img;
    img.width = w;
    img.height = h;
    img.bgra.resize(size_t(w) * h * 4);
    const size_t n = std::min(pix.size(), img.bgra.size());
    std::memcpy(img.bgra.data(), pix.data(), n);
    return img;
}

// A8B8G8R8 stores R, G, B, A in memory; the image is B, G, R, A.
RgbaImage decode_abgr8888(std::span<const u8> src, int w, int h, bool swizzled)
{
    RgbaImage img = decode_argb8888(src, w, h, swizzled);
    for (size_t index = 0; index + 3 < img.bgra.size(); index += 4)
    {
        std::swap(img.bgra[index], img.bgra[index + 2]);
    }
    return img;
}

} // namespace

bool parse_texture_header(std::span<const u8> cpu, TextureHeader& out)
{
    if (cpu.size() < 28)
    {
        return false;
    }
    out.format = static_cast<XboxFormat>(read_u32(cpu, 0));
    out.header_size = read_u32(cpu, 4);
    out.width = read_u16(cpu, 8);
    out.height = read_u16(cpu, 10);
    out.flags = read_u32(cpu, 12);
    out.unknown = cpu[16];
    out.tile_count = cpu[17];
    out.pad = read_u16(cpu, 18);
    out.texture_offset = read_u32(cpu, 20);
    out.data_size = read_u32(cpu, 24);
    return out.header_size == 28 && out.width > 0 && out.height > 0;
}

const char* xbox_format_name(XboxFormat f)
{
    switch (f)
    {
    case XboxFormat::DXT1:
        return "DXT1";
    case XboxFormat::DXT3:
        return "DXT3";
    case XboxFormat::DXT5:
        return "DXT5";
    case XboxFormat::A8R8G8B8:
        return "A8R8G8B8";
    case XboxFormat::X8R8G8B8:
        return "X8R8G8B8";
    case XboxFormat::LinA8R8G8B8:
        return "LIN_A8R8G8B8";
    case XboxFormat::LinX8R8G8B8:
        return "LIN_X8R8G8B8";
    case XboxFormat::A8B8G8R8:
        return "A8B8G8R8";
    case XboxFormat::LinA8B8G8R8:
        return "LIN_A8B8G8R8";
    default:
        return "other";
    }
}

RgbaImage decode_xbox_texture(const TextureHeader& hdr, std::span<const u8> gpu)
{
    if (hdr.texture_offset > gpu.size())
    {
        throw Error("texture offset out of range");
    }
    auto payload = gpu.subspan(hdr.texture_offset, std::min<size_t>(hdr.data_size, gpu.size() - hdr.texture_offset));
    switch (hdr.format)
    {
    case XboxFormat::DXT1:
    case XboxFormat::DXT3:
    case XboxFormat::DXT5:
        return decode_dxt(payload, hdr.width, hdr.height, hdr.format, false);
    case XboxFormat::A8R8G8B8:
    case XboxFormat::X8R8G8B8:
        return decode_argb8888(payload, hdr.width, hdr.height, true);
    case XboxFormat::LinA8R8G8B8:
    case XboxFormat::LinX8R8G8B8:
    {
        RgbaImage image = decode_argb8888(payload, hdr.width, hdr.height, false);
        image.linear = true;
        return image;
    }
    case XboxFormat::A8B8G8R8:
        return decode_abgr8888(payload, hdr.width, hdr.height, true);
    case XboxFormat::LinA8B8G8R8:
    {
        RgbaImage image = decode_abgr8888(payload, hdr.width, hdr.height, false);
        image.linear = true;
        return image;
    }
    default:
        throw Error(std::string("unsupported texture format ") + xbox_format_name(hdr.format));
    }
}

namespace
{
void append_png_u32(std::vector<u8>& output, u32 value)
{
    output.push_back(u8(value >> 24));
    output.push_back(u8(value >> 16));
    output.push_back(u8(value >> 8));
    output.push_back(u8(value));
}

void append_png_chunk(std::vector<u8>& output, const char type[4], std::span<const u8> data)
{
    append_png_u32(output, u32(data.size()));
    const size_t chunk_start = output.size();
    output.insert(output.end(), type, type + 4);
    output.insert(output.end(), data.begin(), data.end());
    const uLong crc = crc32(0, output.data() + chunk_start, uInt(output.size() - chunk_start));
    append_png_u32(output, u32(crc));
}
}

void write_png(const std::filesystem::path& path, const RgbaImage& img)
{
    if (img.width <= 0 || img.height <= 0 || img.width > 0x7FFFFFFF / 4
        || img.bgra.size() != size_t(img.width) * size_t(img.height) * 4)
    {
        throw Error("invalid RGBA image");
    }
    const size_t row_bytes = size_t(img.width) * 4;
    std::vector<u8> filtered((row_bytes + 1) * size_t(img.height));
    for (int y = 0; y < img.height; ++y)
    {
        u8* row = filtered.data() + size_t(y) * (row_bytes + 1);
        row[0] = 0;
        const u8* source = img.bgra.data() + size_t(y) * row_bytes;
        for (int x = 0; x < img.width; ++x)
        {
            row[1 + size_t(x) * 4 + 0] = source[size_t(x) * 4 + 2];
            row[1 + size_t(x) * 4 + 1] = source[size_t(x) * 4 + 1];
            row[1 + size_t(x) * 4 + 2] = source[size_t(x) * 4 + 0];
            row[1 + size_t(x) * 4 + 3] = source[size_t(x) * 4 + 3];
        }
    }
    uLongf compressed_size = compressBound(uLong(filtered.size()));
    std::vector<u8> compressed(static_cast<size_t>(compressed_size));
    const int result = compress2(compressed.data(), &compressed_size, filtered.data(), uLong(filtered.size()),
        Z_BEST_COMPRESSION);
    if (result != Z_OK)
    {
        throw Error("PNG compression failed");
    }
    compressed.resize(size_t(compressed_size));

    std::vector<u8> png{137, 80, 78, 71, 13, 10, 26, 10};
    std::vector<u8> header;
    append_png_u32(header, u32(img.width));
    append_png_u32(header, u32(img.height));
    header.insert(header.end(), {8, 6, 0, 0, 0});
    append_png_chunk(png, "IHDR", header);
    append_png_chunk(png, "IDAT", compressed);
    append_png_chunk(png, "IEND", std::span<const u8>{});
    write_file(path, png);
}

void write_bmp(const std::filesystem::path& path, const RgbaImage& img)
{
    const u32 row_stride = u32((img.width * 3 + 3) & ~3);
    const u32 pixel_bytes = row_stride * u32(img.height);
    std::vector<u8> bmp(54 + pixel_bytes, 0);
    bmp[0] = 'B';
    bmp[1] = 'M';
    const u32 size = u32(bmp.size());
    std::memcpy(bmp.data() + 2, &size, 4);
    u32 off = 54;
    std::memcpy(bmp.data() + 10, &off, 4);
    u32 dib = 40;
    std::memcpy(bmp.data() + 14, &dib, 4);
    i32 w = img.width, h = img.height;
    std::memcpy(bmp.data() + 18, &w, 4);
    std::memcpy(bmp.data() + 22, &h, 4);
    u16 planes = 1, bpp = 24;
    std::memcpy(bmp.data() + 26, &planes, 2);
    std::memcpy(bmp.data() + 28, &bpp, 2);
    std::memcpy(bmp.data() + 34, &pixel_bytes, 4);
    for (int y = 0; y < img.height; ++y)
    {
        u8* dst = bmp.data() + 54 + size_t(img.height - 1 - y) * row_stride;
        const u8* src = img.bgra.data() + size_t(y) * img.width * 4;
        for (int x = 0; x < img.width; ++x)
        {
            dst[x * 3 + 0] = src[x * 4 + 0];
            dst[x * 3 + 1] = src[x * 4 + 1];
            dst[x * 3 + 2] = src[x * 4 + 2];
        }
    }
    write_file(path, bmp);
}

} // namespace gbtg
