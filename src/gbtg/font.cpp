#include "gbtg/font.hpp"

#include "gbtg/resource.hpp"

namespace gbtg
{
namespace
{

RgbaImage decode_frame(std::span<const u8> pixels, int w, int h)
{
    TextureHeader hdr;
    hdr.format = XboxFormat::DXT3;
    hdr.header_size = 28;
    hdr.width = u16(w);
    hdr.height = u16(h);
    hdr.texture_offset = 0;
    hdr.data_size = u32(pixels.size());
    return decode_xbox_texture(hdr, pixels);
}

} // namespace

bool GloopyFont::load(const std::filesystem::path& path)
{
    glyphs_.clear();
    if (!std::filesystem::exists(path))
    {
        return false;
    }
    LooseResource res = parse_loose_resource(read_file(path));
    if (res.header.type != ResourceType::Font || res.cpu.size() < 0x18 || res.gpu_chunks.empty())
    {
        return false;
    }
    const auto& chunk = res.gpu_chunks[0];
    if (size_t(chunk.offset) + chunk.size > res.gpu.size())
    {
        return false;
    }
    const std::span<const u8> cpu = res.cpu;
    const u8* pixels = res.gpu.data() + chunk.offset;
    const size_t pixel_bytes = chunk.size;

    first_ = int(read_u32(cpu, 0));
    const int last = int(read_u32(cpu, 4));
    cell_ = int(read_u32(cpu, 0x0c));
    if (cell_ <= 0)
    {
        cell_ = 32;
    }
    const int count = last - first_ + 1;
    if (count <= 0 || count > 4096 || size_t(0x18 + count * 0x18) > cpu.size())
    {
        return false;
    }

    frame_count_ = 1;
    frame_rate_ = 25.f;
    glyphs_.resize(size_t(count));
    for (int i = 0; i < count; ++i)
    {
        const size_t rec = size_t(0x18 + i * 0x18);
        const u32 tex_off = read_u32(cpu, rec);
        const u32 tag = read_u32(cpu, rec + 4);
        Glyph& glyph = glyphs_[size_t(i)];
        glyph.x0 = int(read_u32(cpu, rec + 8));
        glyph.y0 = int(read_u32(cpu, rec + 0x0c));
        glyph.x1 = int(read_u32(cpu, rec + 0x10));
        glyph.y1 = int(read_u32(cpu, rec + 0x14));
        if (tex_off == 0 || tag == 0xffffffffu || size_t(tex_off) + 28 > cpu.size())
        {
            continue;
        }
        const int tw = int(read_u16(cpu, tex_off + 8));
        const int th = int(read_u16(cpu, tex_off + 10));
        const int rate = int(cpu[tex_off + 16]);
        const int frames = int(cpu[tex_off + 17]);
        const u32 src = read_u32(cpu, tex_off + 20);
        const u32 data_size = read_u32(cpu, tex_off + 24);
        if (tw <= 0 || th <= 0 || data_size == 0 || frames <= 0)
        {
            continue;
        }
        frame_count_ = frames;
        frame_rate_ = float(rate);
        glyph.present = true;
        glyph.frames.resize(size_t(frames));
        for (int f = 0; f < frames; ++f)
        {
            const size_t off = size_t(src) + size_t(f) * data_size;
            if (off + data_size > pixel_bytes)
            {
                continue;
            }
            glyph.frames[size_t(f)] = decode_frame(std::span<const u8>(pixels + off, data_size), tw, th);
        }
    }
    return !glyphs_.empty();
}

int GloopyFont::frame(float time) const
{
    if (frame_count_ <= 1)
    {
        return 0;
    }
    const int n = int(time * frame_rate_);
    const int wrapped = n % frame_count_;
    return wrapped < 0 ? wrapped + frame_count_ : wrapped;
}

const GloopyFont::Glyph* GloopyFont::find(char32_t codepoint) const
{
    const int index = int(codepoint) - first_;
    if (index < 0 || index >= int(glyphs_.size()))
    {
        return nullptr;
    }
    return &glyphs_[size_t(index)];
}

int GloopyFont::advance(char32_t codepoint) const
{
    if (codepoint == 0x20)
    {
        return advance(0x6c);
    }
    const Glyph* glyph = find(codepoint);
    if (!glyph || !glyph->present || glyph->x1 <= glyph->x0)
    {
        return cell_;
    }
    return glyph->x1 - glyph->x0;
}

int GloopyFont::bearing(char32_t codepoint) const
{
    const Glyph* glyph = find(codepoint);
    if (!glyph || !glyph->present)
    {
        return 0;
    }
    return glyph->x0;
}

const RgbaImage* GloopyFont::image(char32_t codepoint, int frame) const
{
    const Glyph* glyph = find(codepoint);
    if (!glyph || !glyph->present || glyph->frames.empty())
    {
        return nullptr;
    }
    const int index = frame_count_ > 0 ? frame % frame_count_ : 0;
    const int wrapped = index < 0 ? index + frame_count_ : index;
    if (wrapped < 0 || wrapped >= int(glyph->frames.size()) || glyph->frames[size_t(wrapped)].width <= 0)
    {
        return nullptr;
    }
    return &glyph->frames[size_t(wrapped)];
}

} // namespace gbtg
