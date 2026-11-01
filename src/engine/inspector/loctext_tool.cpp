#if defined(GBTG_INSPECTOR_APP) && !defined(GBTG_RETAIL_GAME_ONLY)
#include "loctext_tool.h"

#include "gbtg/hash.h"
#include "gbtg/log.h"
#include "gbtg/runtime_localtext.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <map>
#include <set>

namespace gbtg
{
namespace
{
constexpr u32 kLoctextType = 16;
constexpr u32 kLooseMagic = 0x48E58836;
constexpr u32 kLsblHeaderSize = 28;

void put_u16(std::vector<u8>& out, u32 value)
{
    out.push_back(u8(value));
    out.push_back(u8(value >> 8));
}

void put_u32(std::vector<u8>& out, u32 value)
{
    put_u16(out, value);
    put_u16(out, value >> 16);
}

void set_u32(std::vector<u8>& out, size_t at, u32 value)
{
    for (int index = 0; index < 4; ++index)
    {
        out[at + size_t(index)] = u8(value >> (8 * index));
    }
}

u32 get_u32(const std::vector<u8>& bytes, size_t at)
{
    if (at + 4 > bytes.size())
    {
        throw Error("loctext: read past the end");
    }
    return u32(bytes[at]) | u32(bytes[at + 1]) << 8 | u32(bytes[at + 2]) << 16 | u32(bytes[at + 3]) << 24;
}

u16 get_u16(const std::vector<u8>& bytes, size_t at)
{
    if (at + 2 > bytes.size())
    {
        throw Error("loctext: read past the end");
    }
    return u16(bytes[at] | bytes[at + 1] << 8);
}

std::string_view trim(std::string_view text)
{
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())))
    {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())))
    {
        text.remove_suffix(1);
    }
    return text;
}

// UTF-8 to UTF-16 with the \n and \\ escapes. Throws on malformed UTF-8.
std::u16string source_text(std::string_view text)
{
    std::u16string out;
    for (size_t at = 0; at < text.size();)
    {
        if (text[at] == '\\' && at + 1 < text.size() && (text[at + 1] == 'n' || text[at + 1] == '\\'))
        {
            out.push_back(text[at + 1] == 'n' ? u'\n' : u'\\');
            at += 2;
            continue;
        }
        const u8 lead = u8(text[at]);
        const int length = lead < 0x80 ? 1 : (lead >> 5) == 6 ? 2 : (lead >> 4) == 14 ? 3 : (lead >> 3) == 30 ? 4 : 0;
        if (length == 0 || at + size_t(length) > text.size())
        {
            throw Error("loctext source: malformed UTF-8");
        }
        char32_t codepoint = length == 1 ? lead : lead & (0x7F >> length);
        for (int index = 1; index < length; ++index)
        {
            const u8 next = u8(text[at + size_t(index)]);
            if ((next & 0xC0) != 0x80)
            {
                throw Error("loctext source: malformed UTF-8");
            }
            codepoint = (codepoint << 6) | (next & 0x3F);
        }
        if (codepoint >= 0x10000)
        {
            codepoint -= 0x10000;
            out.push_back(char16_t(0xD800 + (codepoint >> 10)));
            out.push_back(char16_t(0xDC00 + (codepoint & 0x3FF)));
        }
        else
        {
            out.push_back(char16_t(codepoint));
        }
        at += size_t(length);
    }
    return out;
}

struct DecodedLoctext
{
    std::vector<LoctextEntry> entries; // In ordinal order.
    std::vector<u16> ordinals;
    std::vector<u8> tables; // The LSBL string table and key table, as stored.
    u32 info_names = 0; // Entries in the INFO name table before LSBL.
};

// The string and key tables of a game loctext file, and its entries in authoring order.
DecodedLoctext decode_loctext(const std::vector<u8>& file)
{
    const size_t chunk = get_u32(file, 16);
    const size_t lsbl = chunk + get_u32(file, chunk);
    if (lsbl + 4 > file.size() || std::memcmp(file.data() + lsbl, "LSBL", 4) != 0)
    {
        throw Error("loctext: no LSBL");
    }
    const size_t table = lsbl + get_u32(file, lsbl + 12);
    const size_t keys = lsbl + get_u32(file, lsbl + 16);
    const u32 count = get_u32(file, table + 4);
    const size_t strings = table + 8 + size_t(count + 1) * 6;
    std::map<u16, std::u16string> texts;
    for (u32 index = 0; index < count; ++index)
    {
        const size_t row = table + 8 + size_t(index) * 6;
        size_t at = strings + size_t(get_u32(file, row + 2)) * 2;
        std::u16string text;
        for (; get_u16(file, at) != 0; at += 2)
        {
            text.push_back(char16_t(get_u16(file, at)));
        }
        texts[get_u16(file, row)] = std::move(text);
    }
    const u32 key_size = get_u32(file, keys);
    const u32 key_count = get_u32(file, keys + 4);
    const size_t names = keys + 8 + size_t(key_count) * 8;
    std::vector<std::pair<u16, LoctextEntry>> ordered;
    for (u32 index = 0; index < key_count; ++index)
    {
        const size_t row = keys + 8 + size_t(index) * 8;
        const size_t name = names + get_u32(file, row + 4);
        if (name >= file.size())
        {
            throw Error("loctext: key name outside the file");
        }
        LoctextEntry entry;
        entry.key = reinterpret_cast<const char*>(file.data() + name);
        entry.text = texts.at(get_u16(file, row));
        ordered.emplace_back(get_u16(file, row + 2), std::move(entry));
    }
    std::sort(ordered.begin(), ordered.end(), [](const auto& a, const auto& b)
    {
        return a.first < b.first;
    });
    DecodedLoctext decoded;
    for (auto& [ordinal, entry] : ordered)
    {
        decoded.ordinals.push_back(ordinal);
        decoded.entries.push_back(std::move(entry));
    }
    decoded.tables.assign(file.begin() + std::ptrdiff_t(table), file.begin() + std::ptrdiff_t(keys + key_size));
    decoded.info_names = get_u32(file, chunk + 8);
    return decoded;
}

std::vector<u8> write_loctext_with_ordinals(const std::vector<LoctextEntry>& entries, const std::vector<u16>& ordinals)
{
    // String ids: the key's hash, or the next free id when an earlier key holds it.
    std::set<u16> used;
    std::vector<u16> ids;
    for (const LoctextEntry& entry : entries)
    {
        u16 id = u16(runtime_localtext_hash(entry.key.c_str()));
        while (used.contains(id) || id == 0xFFFF)
        {
            ++id;
        }
        used.insert(id);
        ids.push_back(id);
    }
    std::vector<size_t> by_id(entries.size());
    for (size_t index = 0; index < by_id.size(); ++index)
    {
        by_id[index] = index;
    }
    std::sort(by_id.begin(), by_id.end(), [&](size_t a, size_t b)
    {
        return ids[a] < ids[b];
    });

    std::vector<u8> chunk;
    put_u32(chunk, 12); // The LSBL block's offset: no INFO name table precedes it.
    put_u32(chunk, 0);
    put_u32(chunk, 0); // INFO name count.
    const size_t lsbl = chunk.size();
    chunk.insert(chunk.end(), {'L', 'S', 'B', 'L'});
    put_u32(chunk, kLsblHeaderSize);
    put_u32(chunk, 4);
    put_u32(chunk, kLsblHeaderSize); // The string table, from LSBL.
    put_u32(chunk, 0); // The key table, from LSBL; set below.
    put_u32(chunk, 0); // No comment chunk.
    put_u32(chunk, 0);

    const size_t table = chunk.size();
    put_u32(chunk, 0); // The string table's size; set below.
    put_u32(chunk, u32(entries.size()));
    u32 characters = 0;
    for (const size_t index : by_id)
    {
        put_u16(chunk, ids[index]);
        put_u32(chunk, characters);
        characters += u32(entries[index].text.size() + 1);
    }
    put_u16(chunk, 0xFFFF);
    put_u32(chunk, characters);
    for (const size_t index : by_id)
    {
        for (const char16_t unit : entries[index].text)
        {
            put_u16(chunk, unit);
        }
        put_u16(chunk, 0);
    }
    put_u16(chunk, 0); // The sentinel row's empty string.
    set_u32(chunk, table, u32(chunk.size() - table));

    const size_t keys = chunk.size();
    set_u32(chunk, lsbl + 16, u32(keys - lsbl));
    put_u32(chunk, 0); // The key table's size; set below.
    put_u32(chunk, u32(entries.size()));
    std::vector<u32> name_offsets;
    u32 name_offset = 0;
    for (const LoctextEntry& entry : entries)
    {
        name_offsets.push_back(name_offset);
        name_offset += u32(entry.key.size() + 1);
    }
    for (const size_t index : by_id)
    {
        put_u16(chunk, ids[index]);
        put_u16(chunk, ordinals[index]);
        put_u32(chunk, name_offsets[index]);
    }
    for (const LoctextEntry& entry : entries)
    {
        chunk.insert(chunk.end(), entry.key.begin(), entry.key.end());
        chunk.push_back(0);
    }
    set_u32(chunk, keys, u32(chunk.size() - keys));

    std::vector<u8> file;
    put_u32(file, kLoctextType);
    put_u32(file, 0);
    put_u32(file, kLooseMagic);
    put_u32(file, 2);
    put_u32(file, 32); // The file chunk.
    put_u32(file, u32(chunk.size()));
    put_u32(file, 0); // No resource chunk.
    put_u32(file, 0);
    file.insert(file.end(), chunk.begin(), chunk.end());
    return file;
}

void write_file(const std::filesystem::path& path, const std::vector<u8>& bytes)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
    if (!output)
    {
        throw Error("could not write " + path.string());
    }
}
}

std::vector<u8> write_loctext(const std::vector<LoctextEntry>& entries)
{
    std::vector<u16> ordinals;
    for (size_t index = 0; index < entries.size(); ++index)
    {
        ordinals.push_back(u16(index + 1));
    }
    return write_loctext_with_ordinals(entries, ordinals);
}

int write_loctext_sources(const std::filesystem::path& source, const std::filesystem::path& out)
{
    size_t written = 0;
    for (const auto& item : std::filesystem::directory_iterator(source))
    {
        if (!item.is_regular_file() || item.path().extension() != ".txt")
        {
            continue;
        }
        const std::string language = item.path().stem().string();
        std::map<std::string, std::vector<LoctextEntry>> resources;
        std::vector<std::string> order;
        std::ifstream input(item.path());
        std::string line;
        std::string resource;
        size_t line_number = 0;
        while (std::getline(input, line))
        {
            ++line_number;
            if (line_number == 1 && line.starts_with("\xEF\xBB\xBF"))
            {
                line.erase(0, 3);
            }
            const std::string_view text = trim(line);
            if (text.empty() || text.front() == ';' || text.front() == '#')
            {
                continue;
            }
            const std::string where = item.path().string() + ":" + std::to_string(line_number);
            if (text.front() == '[' && text.back() == ']')
            {
                resource = std::string(trim(text.substr(1, text.size() - 2)));
                if (!resource.starts_with("aid_loctext_"))
                {
                    throw Error(where + ": a loctext resource is named aid_loctext_...");
                }
                if (!resources.contains(resource))
                {
                    order.push_back(resource);
                }
                resources[resource];
                continue;
            }
            const auto eq = text.find('=');
            if (eq == std::string_view::npos || resource.empty())
            {
                throw Error(where + ": expected [aid_loctext_...] or key = value");
            }
            resources[resource].push_back({std::string(trim(text.substr(0, eq))), source_text(trim(text.substr(eq + 1)))});
        }
        for (const std::string& name : order)
        {
            const auto path = out / "loctext" / language / aid_hash_hex(aid_hash(name));
            write_file(path, write_loctext(resources[name]));
            log::info("loctext %s %s: %zu keys -> %s", language.c_str(), name.c_str(), resources[name].size(),
                path.string().c_str());
            ++written;
        }
    }
    if (written == 0)
    {
        log::error("loctext: no <language>.txt sources in %s", source.string().c_str());
        return 1;
    }
    return 0;
}

int check_loctext_writer(const std::filesystem::path& loctext)
{
    size_t files = 0;
    size_t matched = 0;
    size_t skipped = 0;
    for (const auto& item : std::filesystem::recursive_directory_iterator(loctext))
    {
        if (!item.is_regular_file())
        {
            continue;
        }
        ++files;
        const std::vector<u8> original = read_file(item.path());
        const DecodedLoctext decoded = decode_loctext(original);
        if (decoded.info_names != 0)
        {
            // Files with an INFO name table number their strings another way, which the writer does not produce.
            ++skipped;
            continue;
        }
        const DecodedLoctext rebuilt = decode_loctext(write_loctext_with_ordinals(decoded.entries, decoded.ordinals));
        if (rebuilt.tables == decoded.tables)
        {
            ++matched;
            continue;
        }
        size_t at = 0;
        while (at < rebuilt.tables.size() && at < decoded.tables.size() && rebuilt.tables[at] == decoded.tables[at])
        {
            ++at;
        }
        log::warn("loctext %s: tables differ from byte %zu (%zu vs %zu bytes)", item.path().string().c_str(), at,
            rebuilt.tables.size(), decoded.tables.size());
    }
    const size_t compared = files - skipped;
    log::info("loctext check: %zu of %zu files' string and key tables rebuilt byte for byte (%zu with an INFO name "
        "table skipped)", matched, compared, skipped);
    return compared != 0 && matched == compared ? 0 : 1;
}
}
#endif
