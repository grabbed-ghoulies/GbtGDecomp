#include "gbtg/loctext.hpp"
#include "gbtg/resource.hpp"

#include <algorithm>
#include <cstring>

namespace gbtg
{
namespace
{

std::string utf16le_to_utf8(std::u16string_view w)
{
    std::string out;
    out.reserve(w.size());
    for (char16_t ch : w)
    {
        if (ch < 0x80)
        {
            out.push_back(static_cast<char>(ch));
        }
        else if (ch < 0x800)
        {
            out.push_back(static_cast<char>(0xC0 | (ch >> 6)));
            out.push_back(static_cast<char>(0x80 | (ch & 0x3F)));
        }
        else
        {
            out.push_back(static_cast<char>(0xE0 | (ch >> 12)));
            out.push_back(static_cast<char>(0x80 | ((ch >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (ch & 0x3F)));
        }
    }
    return out;
}

std::string read_cstring(std::span<const u8> b, size_t& off)
{
    std::string s;
    while (off < b.size() && b[off])
    {
        s.push_back(static_cast<char>(b[off++]));
    }
    if (off < b.size())
    {
        ++off;
    }
    return s;
}

std::u16string read_wcstring(std::span<const u8> b, size_t& off)
{
    std::u16string s;
    while (off + 1 < b.size())
    {
        const char16_t ch = char16_t(read_u16(b, off));
        off += 2;
        if (ch == 0)
        {
            break;
        }
        s.push_back(ch);
    }
    return s;
}

} // namespace

LocTable parse_loctext(std::span<const u8> file_bytes, std::string name)
{
    LocTable table;
    table.name = std::move(name);
    ResourceHeader hdr;
    if (!parse_resource_header(file_bytes, hdr))
    {
        throw Error("loctext header");
    }
    if (hdr.file_chunk.offset + hdr.file_chunk.size > file_bytes.size())
    {
        throw Error("loctext file chunk");
    }
    auto chunk = file_bytes.subspan(hdr.file_chunk.offset, hdr.file_chunk.size);
    size_t off = 0;
    if (chunk.size() < 12)
    {
        return table;
    }
    const u32 chunk1_size = read_u32(chunk, off);
    off += 4;
    off += 4; // unknown
    const u32 count1 = read_u32(chunk, off);
    off += 4;
    (void)chunk1_size;
    if (count1 > 4096)
    {
        return table;
    }
    off += count1 * 8; // skip INFO records
    for (u32 i = 0; i < count1 && off < chunk.size(); ++i)
    {
        read_cstring(chunk, off);
    }

    if (off + 4 > chunk.size() || std::memcmp(chunk.data() + off, "LSBL", 4) != 0)
    {
        return table;
    }
    off += 4;
    if (off + 20 > chunk.size())
    {
        return table;
    }
    off += 20; // LSBL header

    off += 4; // chunk2 unknown a
    if (off + 8 > chunk.size())
    {
        return table;
    }
    off += 4; // chunk2 size
    const u32 nstr = read_u32(chunk, off);
    off += 4;
    if (nstr > 20000)
    {
        return table;
    }
    struct Head2
    {
        u16 len;
        u32 id;
    };
    std::vector<Head2> h2(nstr);
    for (u32 i = 0; i < nstr; ++i)
    {
        if (off + 6 > chunk.size())
        {
            return table;
        }
        h2[i].len = read_u16(chunk, off);
        h2[i].id = read_u32(chunk, off + 2);
        off += 6;
    }
    if (off + 6 <= chunk.size())
    {
        off += 6; // extra head
    }
    std::vector<std::string> values;
    values.reserve(nstr);
    for (u32 i = 0; i < nstr; ++i)
    {
        values.push_back(utf16le_to_utf8(read_wcstring(chunk, off)));
    }
    if (off + 2 <= chunk.size())
    {
        read_wcstring(chunk, off); // extra
    }

    if (off + 8 > chunk.size())
    {
        return table;
    }
    off += 4; // chunk3 size
    const u32 nkeys = read_u32(chunk, off);
    off += 4;
    if (nkeys != nstr || nkeys > 20000)
    {
        // still try to read min
    }
    const u32 n = std::min(nstr, nkeys);
    struct Head3
    {
        u16 unknown;
        u16 index;
        u32 unknown2;
    };
    std::vector<Head3> h3(n);
    for (u32 i = 0; i < n; ++i)
    {
        if (off + 8 > chunk.size())
        {
            return table;
        }
        h3[i].unknown = read_u16(chunk, off);
        h3[i].index = read_u16(chunk, off + 2);
        h3[i].unknown2 = read_u32(chunk, off + 4);
        off += 8;
    }
    std::vector<std::string> keys(n);
    for (u32 i = 0; i < n; ++i)
    {
        keys[i] = read_cstring(chunk, off);
    }
    for (u32 i = 0; i < n; ++i)
    {
        u16 idx = h3[i].index;
        if (idx >= 1 && idx <= values.size())
        {
            table.keys_to_utf8[keys[i]] = values[idx - 1];
        }
        else if (i < values.size())
        {
            table.keys_to_utf8[keys[i]] = values[i];
        }
    }
    return table;
}

} // namespace gbtg
