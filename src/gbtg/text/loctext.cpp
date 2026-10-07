#include "gbtg/loctext.h"
#include "gbtg/resource.h"

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
    // Head2 is the runtime stride-6 table 0x00162A30 searches: string id, then
    // a wchar offset from the string blob. The id is not the string's index.
    struct Head2
    {
        u16 id;
        u32 wchar_off;
    };
    std::vector<Head2> h2(nstr);
    for (u32 i = 0; i < nstr; ++i)
    {
        if (off + 6 > chunk.size())
        {
            return table;
        }
        h2[i].id = read_u16(chunk, off);
        h2[i].wchar_off = read_u32(chunk, off + 2);
        off += 6;
    }
    if (off + 6 <= chunk.size())
    {
        off += 6;
    }
    const size_t blob = off;
    for (u32 i = 0; i < nstr && off < chunk.size(); ++i)
    {
        read_wcstring(chunk, off);
    }
    if (off + 2 <= chunk.size())
    {
        read_wcstring(chunk, off);
    }

    if (off + 8 > chunk.size())
    {
        return table;
    }
    off += 4;
    const u32 nkeys = read_u32(chunk, off);
    off += 4;
    if (nkeys > 20000)
    {
        return table;
    }
    // Each key record is (string id, hash, offset into the key blob). Records are
    // sorted by hash, so the offset is what joins a key to its string id.
    struct KeyRec
    {
        u16 string_id;
        u32 key_off;
    };
    std::vector<KeyRec> recs(nkeys);
    for (u32 i = 0; i < nkeys; ++i)
    {
        if (off + 8 > chunk.size())
        {
            return table;
        }
        recs[i].string_id = read_u16(chunk, off);
        recs[i].key_off = read_u32(chunk, off + 4);
        off += 8;
    }
    const size_t keybase = off;
    for (const KeyRec& rec : recs)
    {
        if (size_t(rec.key_off) >= chunk.size() - keybase)
        {
            continue;
        }
        size_t key_at = keybase + rec.key_off;
        const std::string key = read_cstring(chunk, key_at);
        if (key.empty())
        {
            continue;
        }
        const u32 wchar_off = [&]() -> u32 {
            for (const Head2& h : h2)
            {
                if (h.id == rec.string_id)
                {
                    return h.wchar_off;
                }
            }
            return 0xffffffffu;
        }();
        if (wchar_off == 0xffffffffu)
        {
            continue;
        }
        size_t str_at = blob + size_t(wchar_off) * 2;
        if (str_at >= chunk.size())
        {
            continue;
        }
        table.keys_to_utf8[key] = utf16le_to_utf8(read_wcstring(chunk, str_at));
    }
    return table;
}

} // namespace gbtg
