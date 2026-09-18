#include "gbtg/bundle.hpp"
#include "gbtg/hash.hpp"
#include "gbtg/types.hpp"

#include <algorithm>
#include <cstring>

namespace gbtg {

namespace {

struct BnlHeader {
    u16 file_count = 0;
    u8 flag = 0;
    ChunkRef chunks[4]{};
};

BnlHeader read_bnl_header(std::span<const u8> raw) {
    if (raw.size() < 40) {
        throw Error("BNL header truncated");
    }
    BnlHeader h;
    h.file_count = read_u16(raw, 0);
    h.flag = raw[2];
    for (int i = 0; i < 4; ++i) {
        h.chunks[i].offset = read_u32(raw, 8 + i * 8);
        h.chunks[i].size = read_u32(raw, 12 + i * 8);
    }
    return h;
}

}  // namespace

Bundle load_bundle(const std::filesystem::path& path) {
    const auto raw = read_file(path);
    const auto hdr = read_bnl_header(raw);
    const u32 expect = hdr.chunks[0].size + hdr.chunks[1].size + hdr.chunks[2].size + hdr.chunks[3].size;
    auto inflated = zlib_inflate(std::span<const u8>(raw).subspan(40), expect);

    size_t c_off[4];
    size_t c_sz[4];
    for (int i = 0; i < 4; ++i) {
        if (hdr.chunks[i].offset < 40) {
            throw Error("BNL chunk offset < header");
        }
        c_off[i] = size_t(hdr.chunks[i].offset) - 40;
        c_sz[i] = hdr.chunks[i].size;
        if (c_off[i] + c_sz[i] > inflated.size()) {
            throw Error("BNL chunk out of range");
        }
    }

    Bundle b;
    b.path = path.string();
    b.file_count = hdr.file_count;
    if (c_sz[0] < size_t(hdr.file_count) * 160u) {
        throw Error("BNL file table too small");
    }

    std::vector<u8> file_table(inflated.begin() + c_off[0], inflated.begin() + c_off[0] + c_sz[0]);
    std::vector<u8> res_meta(inflated.begin() + c_off[1], inflated.begin() + c_off[1] + c_sz[1]);
    b.cpu_heap.assign(inflated.begin() + c_off[3], inflated.begin() + c_off[3] + c_sz[3]);
    b.gpu_heap.assign(inflated.begin() + c_off[2], inflated.begin() + c_off[2] + c_sz[2]);
    inflated.clear();
    inflated.shrink_to_fit();

    auto table = std::span<const u8>(file_table);
    auto meta = std::span<const u8>(res_meta);
    b.entries.reserve(hdr.file_count);

    for (u16 i = 0; i < hdr.file_count; ++i) {
        const size_t base = size_t(i) * 160;
        char namebuf[129]{};
        std::memcpy(namebuf, file_table.data() + base, 128);
        BundleEntry e;
        e.name = namebuf;
        if (auto z = e.name.find('\0'); z != std::string::npos) {
            e.name.resize(z);
        }
        e.hash = aid_hash(e.name);
        parse_resource_header(table.subspan(base + 128, 32), e.header);
        e.type = e.header.type;

        const auto& fc = e.header.file_chunk;
        const size_t cpu_off = fc.offset;
        const size_t cpu_end = cpu_off + fc.size;
        if (fc.size && cpu_end <= b.cpu_heap.size() && cpu_off <= cpu_end) {
            e.cpu = std::span<const u8>(b.cpu_heap).subspan(cpu_off, fc.size);
        }

        if (e.header.resource_chunk.size >= 8) {
            const size_t rh = e.header.resource_chunk.offset;
            if (rh + 8 <= res_meta.size()) {
                const u32 count = read_u32(meta, rh + 4);
                if (count < 4096 && rh + 8 + size_t(count) * 8 <= res_meta.size()) {
                    e.gpu_chunks.resize(count);
                    for (u32 k = 0; k < count; ++k) {
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

const BundleEntry* find_entry(const Bundle& b, std::string_view name) {
    for (const auto& e : b.entries) {
        if (e.name == name) {
            return &e;
        }
    }
    return nullptr;
}

std::vector<const BundleEntry*> entries_of_type(const Bundle& b, ResourceType type) {
    std::vector<const BundleEntry*> out;
    for (const auto& e : b.entries) {
        if (e.type == type) {
            out.push_back(&e);
        }
    }
    return out;
}

}  // namespace gbtg
