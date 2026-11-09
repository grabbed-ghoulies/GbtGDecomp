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

} // namespace

void swizzle_nv2a_into(std::span<u8> destination, std::span<const u8> linear, u32 width, u32 height, u32 bpp)
{
    u32 mask_x = 0, mask_y = 0, bit = 1, mask_bit = 1;
    for (;;)
    {
        bool done = true;
        if (bit < width)
        {
            mask_x |= mask_bit;
            mask_bit <<= 1;
            done = false;
        }
        if (bit < height)
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
    u32 off_y = 0;
    for (u32 y = 0; y < height; ++y)
    {
        u32 off_x = 0;
        for (u32 x = 0; x < width; ++x)
        {
            const size_t to = size_t(off_y + off_x) * bpp;
            const size_t from = (size_t(y) * width + x) * bpp;
            if (to + bpp <= destination.size() && from + bpp <= linear.size())
            {
                std::memcpy(destination.data() + to, linear.data() + from, bpp);
            }
            off_x = (off_x - mask_x) & mask_x;
        }
        off_y = (off_y - mask_y) & mask_y;
    }
}

namespace
{

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

std::vector<u8> encode_png(const RgbaImage& img)
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
    return png;
}

void write_png(const std::filesystem::path& path, const RgbaImage& img)
{
    write_file(path, encode_png(img));
}

RgbaImage resize_image(const RgbaImage& image, int width, int height)
{
    if (image.width <= 0 || image.height <= 0 || width <= 0 || height <= 0)
    {
        throw Error("cannot resize an empty image");
    }
    if (image.width == width && image.height == height)
    {
        return image;
    }
    RgbaImage out;
    out.width = width;
    out.height = height;
    out.bgra.resize(size_t(width) * height * 4);
    for (int y = 0; y < height; ++y)
    {
        const float sy = std::clamp((float(y) + .5f) * float(image.height) / float(height) - .5f, 0.f,
            float(image.height - 1));
        const int y0 = int(sy);
        const int y1 = std::min(y0 + 1, image.height - 1);
        const float fy = sy - float(y0);
        for (int x = 0; x < width; ++x)
        {
            const float sx = std::clamp((float(x) + .5f) * float(image.width) / float(width) - .5f, 0.f,
                float(image.width - 1));
            const int x0 = int(sx);
            const int x1 = std::min(x0 + 1, image.width - 1);
            const float fx = sx - float(x0);
            for (int channel = 0; channel < 4; ++channel)
            {
                const auto at = [&](int px, int py)
                {
                    return float(image.bgra[(size_t(py) * image.width + px) * 4 + size_t(channel)]);
                };
                const float top = at(x0, y0) + (at(x1, y0) - at(x0, y0)) * fx;
                const float bottom = at(x0, y1) + (at(x1, y1) - at(x0, y1)) * fx;
                out.bgra[(size_t(y) * width + x) * 4 + size_t(channel)] = u8(std::lround(top + (bottom - top) * fy));
            }
        }
    }
    return out;
}

namespace
{
u16 pack_rgb565(float r, float g, float b)
{
    const auto q = [](float value, int levels)
    {
        return u16(std::clamp(int(std::lround(value * float(levels) / 255.f)), 0, levels));
    };
    return u16(q(r, 31) << 11 | q(g, 63) << 5 | q(b, 31));
}
}

std::vector<u8> encode_dxt1(const RgbaImage& image)
{
    const int bw = (image.width + 3) / 4;
    const int bh = (image.height + 3) / 4;
    std::vector<u8> out(size_t(bw) * bh * 8);
    for (int by = 0; by < bh; ++by)
    {
        for (int bx = 0; bx < bw; ++bx)
        {
            float pixels[16][3];
            bool transparent[16];
            bool any_transparent = false;
            for (int i = 0; i < 16; ++i)
            {
                const int x = std::min(bx * 4 + (i & 3), image.width - 1);
                const int y = std::min(by * 4 + (i >> 2), image.height - 1);
                const u8* p = image.bgra.data() + (size_t(y) * image.width + x) * 4;
                pixels[i][0] = p[2];
                pixels[i][1] = p[1];
                pixels[i][2] = p[0];
                transparent[i] = p[3] < 128;
                any_transparent = any_transparent || transparent[i];
            }
            // Endpoints along the principal axis of the opaque texels.
            float mean[3]{};
            int opaque = 0;
            for (int i = 0; i < 16; ++i)
            {
                if (!transparent[i])
                {
                    for (int c = 0; c < 3; ++c)
                    {
                        mean[c] += pixels[i][c];
                    }
                    ++opaque;
                }
            }
            u8* block = out.data() + (size_t(by) * bw + bx) * 8;
            if (opaque == 0)
            {
                // All transparent: the three-colour mode's index 3.
                block[0] = block[1] = block[2] = block[3] = 0;
                block[4] = block[5] = block[6] = block[7] = 0xFF;
                continue;
            }
            for (float& value : mean)
            {
                value /= float(opaque);
            }
            float cov[6]{};
            for (int i = 0; i < 16; ++i)
            {
                if (transparent[i])
                {
                    continue;
                }
                const float d[3]{pixels[i][0] - mean[0], pixels[i][1] - mean[1], pixels[i][2] - mean[2]};
                cov[0] += d[0] * d[0];
                cov[1] += d[0] * d[1];
                cov[2] += d[0] * d[2];
                cov[3] += d[1] * d[1];
                cov[4] += d[1] * d[2];
                cov[5] += d[2] * d[2];
            }
            float axis[3]{1.f, 1.f, 1.f};
            for (int iteration = 0; iteration < 8; ++iteration)
            {
                const float next[3]{cov[0] * axis[0] + cov[1] * axis[1] + cov[2] * axis[2],
                    cov[1] * axis[0] + cov[3] * axis[1] + cov[4] * axis[2],
                    cov[2] * axis[0] + cov[4] * axis[1] + cov[5] * axis[2]};
                const float length = std::sqrt(next[0] * next[0] + next[1] * next[1] + next[2] * next[2]);
                if (length < 1e-6f)
                {
                    break;
                }
                for (int c = 0; c < 3; ++c)
                {
                    axis[c] = next[c] / length;
                }
            }
            float low = 1e9f;
            float high = -1e9f;
            for (int i = 0; i < 16; ++i)
            {
                if (transparent[i])
                {
                    continue;
                }
                const float t = (pixels[i][0] - mean[0]) * axis[0] + (pixels[i][1] - mean[1]) * axis[1]
                    + (pixels[i][2] - mean[2]) * axis[2];
                low = std::min(low, t);
                high = std::max(high, t);
            }
            u16 c0 = pack_rgb565(mean[0] + axis[0] * high, mean[1] + axis[1] * high, mean[2] + axis[2] * high);
            u16 c1 = pack_rgb565(mean[0] + axis[0] * low, mean[1] + axis[1] * low, mean[2] + axis[2] * low);
            // Four-colour blocks need c0 > c1; three-colour (transparent) blocks need c0 <= c1.
            if (any_transparent ? c0 > c1 : c0 < c1)
            {
                std::swap(c0, c1);
            }
            if (!any_transparent && c0 == c1)
            {
                if (c1 > 0)
                {
                    --c1;
                }
                else
                {
                    ++c0;
                }
            }
            u8 palette[4][3]{};
            decode_rgb565(c0, palette[0][0], palette[0][1], palette[0][2]);
            decode_rgb565(c1, palette[1][0], palette[1][1], palette[1][2]);
            const int colours = any_transparent ? 3 : 4;
            for (int c = 0; c < 3; ++c)
            {
                if (any_transparent)
                {
                    palette[2][c] = u8((palette[0][c] + palette[1][c]) / 2);
                }
                else
                {
                    palette[2][c] = u8((2 * palette[0][c] + palette[1][c]) / 3);
                    palette[3][c] = u8((palette[0][c] + 2 * palette[1][c]) / 3);
                }
            }
            u32 indices = 0;
            for (int i = 0; i < 16; ++i)
            {
                u32 best = 3;
                if (!transparent[i])
                {
                    float best_distance = 1e30f;
                    for (int candidate = 0; candidate < colours; ++candidate)
                    {
                        float distance = 0.f;
                        for (int c = 0; c < 3; ++c)
                        {
                            const float d = pixels[i][c] - float(palette[candidate][c]);
                            distance += d * d;
                        }
                        if (distance < best_distance)
                        {
                            best_distance = distance;
                            best = u32(candidate);
                        }
                    }
                }
                indices |= best << (i * 2);
            }
            block[0] = u8(c0);
            block[1] = u8(c0 >> 8);
            block[2] = u8(c1);
            block[3] = u8(c1 >> 8);
            block[4] = u8(indices);
            block[5] = u8(indices >> 8);
            block[6] = u8(indices >> 16);
            block[7] = u8(indices >> 24);
        }
    }
    return out;
}

RgbaImage decode_png(std::span<const u8> png)
{
    static constexpr u8 kSignature[8]{137, 80, 78, 71, 13, 10, 26, 10};
    if (png.size() < 8 || std::memcmp(png.data(), kSignature, 8) != 0)
    {
        throw Error("not a PNG image");
    }
    const auto be32 = [&](size_t at)
    {
        if (at + 4 > png.size())
        {
            throw Error("truncated PNG image");
        }
        return u32(png[at]) << 24 | u32(png[at + 1]) << 16 | u32(png[at + 2]) << 8 | u32(png[at + 3]);
    };
    u32 width = 0;
    u32 height = 0;
    u8 depth = 0;
    u8 color = 0;
    u8 interlace = 0;
    std::vector<u8> idat;
    std::vector<u8> palette;
    std::vector<u8> palette_alpha;
    size_t at = 8;
    while (at + 8 <= png.size())
    {
        const u32 length = be32(at);
        const std::string_view type(reinterpret_cast<const char*>(png.data() + at + 4), 4);
        const size_t data = at + 8;
        if (data + length + 4 > png.size())
        {
            throw Error("truncated PNG chunk");
        }
        if (type == "IHDR")
        {
            width = be32(data);
            height = be32(data + 4);
            depth = png[data + 8];
            color = png[data + 9];
            interlace = png[data + 12];
        }
        else if (type == "PLTE")
        {
            palette.assign(png.begin() + std::ptrdiff_t(data), png.begin() + std::ptrdiff_t(data + length));
        }
        else if (type == "tRNS")
        {
            palette_alpha.assign(png.begin() + std::ptrdiff_t(data), png.begin() + std::ptrdiff_t(data + length));
        }
        else if (type == "IDAT")
        {
            idat.insert(idat.end(), png.begin() + std::ptrdiff_t(data), png.begin() + std::ptrdiff_t(data + length));
        }
        else if (type == "IEND")
        {
            break;
        }
        at = data + length + 4;
    }
    if (width == 0 || height == 0 || width > 16384 || height > 16384)
    {
        throw Error("PNG image has an unsupported size");
    }
    const u32 channels = color == 0 ? 1 : color == 2 ? 3 : color == 3 ? 1 : color == 4 ? 2 : color == 6 ? 4 : 0;
    if (channels == 0)
    {
        throw Error("PNG image has an unsupported colour type");
    }
    // 8 and 16 bits a sample (16 keeps its high byte), and 1, 2 or 4 bits for grey and palette images.
    const bool packed = depth == 1 || depth == 2 || depth == 4;
    if (interlace != 0 || !(depth == 8 || depth == 16 || (packed && (color == 0 || color == 3)))
        || (color == 3 && depth == 16))
    {
        throw Error("PNG image uses an unsupported bit depth or interlacing");
    }
    const size_t row_bytes = (size_t(width) * channels * depth + 7) / 8;
    const size_t step = std::max<size_t>(1, size_t(channels) * depth / 8); // The filters' bytes per pixel.
    std::vector<u8> raw((row_bytes + 1) * height);
    uLongf raw_size = uLongf(raw.size());
    if (uncompress(raw.data(), &raw_size, idat.data(), uLong(idat.size())) != Z_OK || raw_size != raw.size())
    {
        throw Error("PNG image data does not inflate");
    }
    std::vector<u8> pixels(row_bytes * height);
    for (u32 y = 0; y < height; ++y)
    {
        const u8 filter = raw[y * (row_bytes + 1)];
        const u8* line = raw.data() + y * (row_bytes + 1) + 1;
        u8* out = pixels.data() + y * row_bytes;
        const u8* previous = y == 0 ? nullptr : pixels.data() + (y - 1) * row_bytes;
        for (size_t x = 0; x < row_bytes; ++x)
        {
            const int a = x >= step ? out[x - step] : 0;
            const int b = previous != nullptr ? previous[x] : 0;
            const int c = previous != nullptr && x >= step ? previous[x - step] : 0;
            int value = line[x];
            switch (filter)
            {
            case 0:
                break;
            case 1:
                value += a;
                break;
            case 2:
                value += b;
                break;
            case 3:
                value += (a + b) / 2;
                break;
            case 4:
            {
                const int p = a + b - c;
                const int pa = std::abs(p - a);
                const int pb = std::abs(p - b);
                const int pc = std::abs(p - c);
                value += pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
                break;
            }
            default:
                throw Error("PNG image uses an unknown filter");
            }
            out[x] = u8(value);
        }
    }
    if (depth != 8)
    {
        // To one byte a sample: a 16-bit sample's high byte; packed samples unpacked (grey scaled to 0..255).
        std::vector<u8> samples(size_t(width) * channels * height);
        for (u32 y = 0; y < height; ++y)
        {
            const u8* row = pixels.data() + y * row_bytes;
            for (size_t i = 0; i < size_t(width) * channels; ++i)
            {
                u8 value;
                if (depth == 16)
                {
                    value = row[i * 2];
                }
                else
                {
                    const size_t bit = i * depth;
                    const u32 max = (1u << depth) - 1;
                    const u32 raw = (row[bit / 8] >> (8 - depth - bit % 8)) & max;
                    value = u8(color == 0 ? raw * 255 / max : raw);
                }
                samples[y * size_t(width) * channels + i] = value;
            }
        }
        pixels = std::move(samples);
    }
    RgbaImage image;
    image.width = int(width);
    image.height = int(height);
    image.bgra.resize(size_t(width) * height * 4);
    for (size_t index = 0; index < size_t(width) * height; ++index)
    {
        const u8* source = pixels.data() + index * channels;
        u8 r, g, b, alpha = 255;
        switch (color)
        {
        case 0:
            r = g = b = source[0];
            break;
        case 4:
            r = g = b = source[0];
            alpha = source[1];
            break;
        case 2:
            r = source[0];
            g = source[1];
            b = source[2];
            break;
        case 3:
        {
            const size_t entry = source[0];
            if (entry * 3 + 2 >= palette.size())
            {
                throw Error("PNG palette index out of range");
            }
            r = palette[entry * 3];
            g = palette[entry * 3 + 1];
            b = palette[entry * 3 + 2];
            alpha = entry < palette_alpha.size() ? palette_alpha[entry] : 255;
            break;
        }
        default:
            r = source[0];
            g = source[1];
            b = source[2];
            alpha = source[3];
            break;
        }
        u8* target = image.bgra.data() + index * 4;
        target[0] = b;
        target[1] = g;
        target[2] = r;
        target[3] = alpha;
    }
    return image;
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
