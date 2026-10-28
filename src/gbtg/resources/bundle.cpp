#include "gbtg/bundle.h"
#include "gbtg/hash.h"
#include "gbtg/types.h"

#include <cctype>
#include <algorithm>
#include <cstring>

namespace gbtg
{

namespace
{

struct BnlHeader
{
    u16 file_count = 0;
    u8 flag = 0;
    ChunkRef chunks[4]{};
};

BnlHeader read_bnl_header(std::span<const u8> raw)
{
    if (raw.size() < 40)
    {
        throw Error("BNL header truncated");
    }
    BnlHeader h;
    h.file_count = read_u16(raw, 0);
    h.flag = raw[2];
    for (int i = 0; i < 4; ++i)
    {
        h.chunks[i].offset = read_u32(raw, 8 + i * 8);
        h.chunks[i].size = read_u32(raw, 12 + i * 8);
    }
    return h;
}

} // namespace

Bundle load_bundle(const std::filesystem::path& path)
{
    const auto raw = read_file(path);
    const auto hdr = read_bnl_header(raw);
    const u32 expect = hdr.chunks[0].size + hdr.chunks[1].size + hdr.chunks[2].size + hdr.chunks[3].size;
    auto inflated = zlib_inflate(std::span<const u8>(raw).subspan(40), expect);

    size_t c_off[4];
    size_t c_sz[4];
    for (int i = 0; i < 4; ++i)
    {
        if (hdr.chunks[i].offset < 40)
        {
            throw Error("BNL chunk offset < header");
        }
        c_off[i] = size_t(hdr.chunks[i].offset) - 40;
        c_sz[i] = hdr.chunks[i].size;
        if (c_off[i] + c_sz[i] > inflated.size())
        {
            throw Error("BNL chunk out of range");
        }
    }

    Bundle b;
    b.path = path.string();
    b.file_count = hdr.file_count;
    b.flag = hdr.flag;
    if (c_sz[0] < size_t(hdr.file_count) * 160u)
    {
        throw Error("BNL file table too small");
    }

    b.file_table.assign(inflated.begin() + c_off[0], inflated.begin() + c_off[0] + c_sz[0]);
    b.res_meta.assign(inflated.begin() + c_off[1], inflated.begin() + c_off[1] + c_sz[1]);
    b.cpu_heap.assign(inflated.begin() + c_off[3], inflated.begin() + c_off[3] + c_sz[3]);
    b.gpu_heap.assign(inflated.begin() + c_off[2], inflated.begin() + c_off[2] + c_sz[2]);
    inflated.clear();
    inflated.shrink_to_fit();

    auto table = std::span<const u8>(b.file_table);
    auto meta = std::span<const u8>(b.res_meta);
    b.entries.reserve(hdr.file_count);

    for (u16 i = 0; i < hdr.file_count; ++i)
    {
        const size_t base = size_t(i) * 160;
        char namebuf[129]{};
        std::memcpy(namebuf, b.file_table.data() + base, 128);
        BundleEntry e;
        e.name = namebuf;
        if (auto z = e.name.find('\0'); z != std::string::npos)
        {
            e.name.resize(z);
        }
        e.hash = aid_hash(e.name);
        parse_resource_header(table.subspan(base + 128, 32), e.header);
        e.type = e.header.type;

        const auto& fc = e.header.file_chunk;
        const size_t cpu_off = fc.offset;
        const size_t cpu_end = cpu_off + fc.size;
        if (fc.size && cpu_end <= b.cpu_heap.size() && cpu_off <= cpu_end)
        {
            e.cpu = std::span<const u8>(b.cpu_heap).subspan(cpu_off, fc.size);
        }

        if (e.header.resource_chunk.size >= 8)
        {
            const size_t rh = e.header.resource_chunk.offset;
            if (rh + 8 <= b.res_meta.size())
            {
                const u32 count = read_u32(meta, rh + 4);
                if (count < 4096 && rh + 8 + size_t(count) * 8 <= b.res_meta.size())
                {
                    e.gpu_chunks.resize(count);
                    for (u32 k = 0; k < count; ++k)
                    {
                        e.gpu_chunks[k].offset = read_u32(meta, rh + 8 + k * 8);
                        e.gpu_chunks[k].size = read_u32(meta, rh + 12 + k * 8);
                    }
                }
            }
        }
        e.gpu = std::span<const u8>(b.gpu_heap);
        b.entries.push_back(std::move(e));
    }
    return b;
}

const BundleEntry* find_entry(const Bundle& b, std::string_view name)
{
    // An AID's identity is its hash, which folds case (aid_hash): scene 3b7's skeleton table asks for
    // ..._noegganims_turnStep, which its bundle lists as ..._noegganims_turnstep.
    const auto same = [name](std::string_view entry)
    {
        if (entry.size() != name.size())
        {
            return false;
        }
        for (size_t i = 0; i < name.size(); ++i)
        {
            if (std::tolower(static_cast<unsigned char>(entry[i])) != std::tolower(static_cast<unsigned char>(name[i])))
            {
                return false;
            }
        }
        return true;
    };
    for (const auto& e : b.entries)
    {
        if (e.name == name)
        {
            return &e;
        }
    }
    for (const auto& e : b.entries)
    {
        if (same(e.name))
        {
            return &e;
        }
    }
    return nullptr;
}

std::vector<const BundleEntry*> entries_of_type(const Bundle& b, ResourceType type)
{
    std::vector<const BundleEntry*> out;
    for (const auto& e : b.entries)
    {
        if (e.type == type)
        {
            out.push_back(&e);
        }
    }
    return out;
}

std::vector<std::string> parse_aidlist(std::span<const u8> cpu)
{
    std::vector<std::string> names;
    constexpr size_t kRec = 128;
    names.reserve(cpu.size() / kRec);
    for (size_t i = 0; i + kRec <= cpu.size(); i += kRec)
    {
        if (cpu[i] == 0)
        {
            continue;
        }
        char buf[kRec + 1]{};
        std::memcpy(buf, cpu.data() + i, kRec);
        names.emplace_back(buf);
    }
    return names;
}

void save_bundle(const Bundle& b, const std::filesystem::path& path)
{
    std::vector<u8> inflated;
    inflated.reserve(b.file_table.size() + b.res_meta.size() + b.gpu_heap.size() + b.cpu_heap.size());
    inflated.insert(inflated.end(), b.file_table.begin(), b.file_table.end());
    inflated.insert(inflated.end(), b.res_meta.begin(), b.res_meta.end());
    inflated.insert(inflated.end(), b.gpu_heap.begin(), b.gpu_heap.end());
    inflated.insert(inflated.end(), b.cpu_heap.begin(), b.cpu_heap.end());
    const auto comp = zlib_deflate(inflated);

    std::vector<u8> out(40 + comp.size(), 0);
    out[0] = u8(b.file_count);
    out[1] = u8(b.file_count >> 8);
    out[2] = b.flag;
    u32 off = 40;
    const u32 sizes[4] = {u32(b.file_table.size()), u32(b.res_meta.size()), u32(b.gpu_heap.size()),
                          u32(b.cpu_heap.size())};
    for (int i = 0; i < 4; ++i)
    {
        write_u32(out, 8 + i * 8, off);
        write_u32(out, 12 + i * 8, sizes[i]);
        off += sizes[i];
    }
    std::memcpy(out.data() + 40, comp.data(), comp.size());
    write_file(path, out);
}

void patch_aidlist_prefix(Bundle& b, std::string_view list_name, std::string_view new_aid, size_t replace_count)
{
    const BundleEntry* e = find_entry(b, list_name);
    if (!e)
    {
        throw Error("aidlist not found: " + std::string(list_name));
    }
    if (e->type != ResourceType::AidList)
    {
        throw Error("not an aidlist: " + std::string(list_name));
    }
    const size_t rec = 128;
    if (new_aid.size() >= rec)
    {
        throw Error("AID name too long for aidlist slot");
    }
    const size_t cpu_off = e->header.file_chunk.offset;
    const size_t cpu_sz = e->header.file_chunk.size;
    if (cpu_off + cpu_sz > b.cpu_heap.size())
    {
        throw Error("aidlist cpu out of range");
    }
    const size_t n = cpu_sz / rec;
    const size_t use = std::min(replace_count, n);
    for (size_t i = 0; i < use; ++i)
    {
        u8* slot = b.cpu_heap.data() + cpu_off + i * rec;
        std::memset(slot, 0, rec);
        std::memcpy(slot, new_aid.data(), new_aid.size());
    }
}

} // namespace gbtg
