#include "gbtg/model.hpp"

#include <algorithm>
#include <cstring>
#include <vector>

namespace gbtg {
namespace {

struct VertStream {
    size_t cpu_off = 0;
    u32 n = 0;
    std::vector<Vec3> positions;
    std::vector<Vec2> uvs;
};

std::vector<VertStream> parse_vertex_streams(std::span<const u8> cpu, std::span<const u8> gpu) {
    std::vector<VertStream> streams;
    if (cpu.size() < 24) {
        return streams;
    }
    for (size_t off = 0; off + 24 <= cpu.size(); ++off) {
        const u32 packed = read_u32(cpu, off);
        const u8 info_size = u8(packed & 0xFF);
        const u8 info_type = u8((packed >> 8) & 0xFF);
        if (info_type != 0x09 || info_size != 12) {
            continue;
        }
        const u32 goff = read_u32(cpu, off + 16);
        const u32 gsz = read_u32(cpu, off + 20);
        if (gsz < 12 || gsz % 12 != 0 || goff > gpu.size() || goff + gsz > gpu.size()) {
            continue;
        }
        const u32 n = gsz / 12;
        if (n < 3 || n > 200000) {
            continue;
        }
        VertStream s;
        s.cpu_off = off;
        s.n = n;
        s.positions.resize(n);
        Vec3 bmin{1e9f, 1e9f, 1e9f};
        Vec3 bmax{-1e9f, -1e9f, -1e9f};
        for (u32 v = 0; v < n; ++v) {
            Vec3 p{read_f32(gpu, goff + v * 12 + 0), read_f32(gpu, goff + v * 12 + 4),
                   read_f32(gpu, goff + v * 12 + 8)};
            s.positions[v] = p;
            bmin.x = std::min(bmin.x, p.x);
            bmin.y = std::min(bmin.y, p.y);
            bmin.z = std::min(bmin.z, p.z);
            bmax.x = std::max(bmax.x, p.x);
            bmax.y = std::max(bmax.y, p.y);
            bmax.z = std::max(bmax.z, p.z);
        }
        const float extent = std::max({bmax.x - bmin.x, bmax.y - bmin.y, bmax.z - bmin.z});
        // Blendshape deltas sit in extra 0x09 streams with a near-zero bbox.
        if (extent < 2.5f) {
            continue;
        }
        streams.push_back(std::move(s));
    }
    std::vector<std::pair<u32, std::vector<Vec2>>> uv_streams;
    for (size_t off = 0; off + 24 <= cpu.size(); ++off) {
        const u32 packed = read_u32(cpu, off);
        const u8 info_size = u8(packed & 0xFF);
        const u8 info_type = u8((packed >> 8) & 0xFF);
        if (info_type != 0x0D || info_size != 8) {
            continue;
        }
        const u32 goff = read_u32(cpu, off + 16);
        const u32 gsz = read_u32(cpu, off + 20);
        if (!gsz || gsz % 8 != 0 || goff + gsz > gpu.size()) {
            continue;
        }
        const u32 n = gsz / 8;
        if (n > 200000) {
            continue;
        }
        std::vector<Vec2> uvs(n);
        for (u32 v = 0; v < n; ++v) {
            uvs[v].u = read_f32(gpu, goff + v * 8 + 0);
            uvs[v].v = read_f32(gpu, goff + v * 8 + 4);
        }
        uv_streams.emplace_back(n, std::move(uvs));
    }
    for (auto& s : streams) {
        for (auto& uv : uv_streams) {
            if (uv.first == s.n && s.uvs.empty()) {
                s.uvs = std::move(uv.second);
                uv.first = 0;
                break;
            }
        }
    }
    return streams;
}

float dist2(Vec3 a, Vec3 b) {
    const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}

void emit_indices(Mesh& mesh, const std::vector<u16>& idx, u32 prim, u32 base, u32 stream_n) {
    auto push = [&](u16 a, u16 b, u16 c) {
        if (a == b || b == c || a == c) {
            return;
        }
        if (a >= stream_n || b >= stream_n || c >= stream_n) {
            return;
        }
        const u32 ia = base + a, ib = base + b, ic = base + c;
        if (ia >= mesh.positions.size() || ib >= mesh.positions.size() || ic >= mesh.positions.size()) {
            return;
        }
        const Vec3 pa = mesh.positions[ia], pb = mesh.positions[ib], pc = mesh.positions[ic];
        const Vec3 ab{pb.x - pa.x, pb.y - pa.y, pb.z - pa.z};
        const Vec3 ac{pc.x - pa.x, pc.y - pa.y, pc.z - pa.z};
        const Vec3 cr{ab.y * ac.z - ab.z * ac.y, ab.z * ac.x - ab.x * ac.z, ab.x * ac.y - ab.y * ac.x};
        const float cross2 = cr.x * cr.x + cr.y * cr.y + cr.z * cr.z;
        const float longest2 = std::max({dist2(pa, pb), dist2(pb, pc), dist2(pc, pa)});
        const float min_height = 0.005f * longest2;
        if (cross2 < 1e-12f || (longest2 > 1e-8f && cross2 < min_height * min_height)) {
            return;
        }
        mesh.indices.push_back(u16(ia));
        mesh.indices.push_back(u16(ib));
        mesh.indices.push_back(u16(ic));
    };
    if (idx.size() < 3) {
        return;
    }
    // NV097_SET_BEGIN_END: 5=triangles, 6=strip, 7=fan. Unknown → strip unless count%3==0.
    if (prim == 5 || (prim != 6 && prim != 7 && idx.size() % 3 == 0)) {
        for (size_t i = 0; i + 2 < idx.size(); i += 3) {
            push(idx[i], idx[i + 1], idx[i + 2]);
        }
        return;
    }
    if (prim == 7) {
        for (size_t i = 1; i + 1 < idx.size(); ++i) {
            push(idx[0], idx[i], idx[i + 1]);
        }
        return;
    }
    for (size_t i = 0; i + 2 < idx.size(); ++i) {
        if (i & 1) {
            push(idx[i + 1], idx[i], idx[i + 2]);
        } else {
            push(idx[i], idx[i + 1], idx[i + 2]);
        }
    }
}

void parse_push_buffers(std::span<const u8> cpu, Mesh& mesh, const std::vector<VertStream>& streams,
                        const std::vector<u32>& bases) {
    if (streams.empty()) {
        return;
    }
    auto stream_at = [&](size_t pb_off) -> size_t {
        size_t found = 0;
        for (size_t i = 0; i < streams.size(); ++i) {
            if (streams[i].cpu_off <= pb_off) {
                found = i;
            }
        }
        return found;
    };
    auto scan = [&](const char* tag) {
        const size_t tlen = std::strlen(tag);
        for (size_t i = 36; i + tlen + 28 < cpu.size(); ++i) {
            if (std::memcmp(cpu.data() + i, tag, tlen) != 0) {
                continue;
            }
            const u32 count = read_u32(cpu, i + 16);
            const u32 this_ptr = read_u32(cpu, i + 20);
            const u32 prim = read_u32(cpu, i + 24);
            // Scene-graph nodes: 36-byte header then a 16-byte tag. `this_ptr` points at the header.
            if (this_ptr != i - 36 || (prim != 5 && prim != 6 && prim != 7)) {
                continue;
            }
            const size_t idx_off = i + 28;
            if (count < 3 || count >= 500000 || idx_off + count * 2 > cpu.size()) {
                continue;
            }
            std::vector<u16> idx;
            idx.reserve(count);
            for (u32 k = 0; k < count; ++k) {
                idx.push_back(read_u16(cpu, idx_off + k * 2));
            }
            const size_t si = stream_at(i);
            emit_indices(mesh, idx, prim, bases[si], streams[si].n);
        }
    };
    scan("ndPushBuffer");
    scan("ndBGPushBuffer");
}

void parse_embedded_textures(std::span<const u8> cpu, std::span<const u8> gpu, Mesh& mesh) {
    if (cpu.size() < 32) {
        return;
    }
    const u32 tail_off = read_u32(cpu, 0);
    const u32 tail_count = read_u32(cpu, 4);
    if (tail_count == 0 || tail_count > 256 || size_t(tail_off) + size_t(tail_count) * 8 > cpu.size()) {
        return;
    }
    for (u32 i = 0; i < tail_count; ++i) {
        const u32 type = read_u32(cpu, tail_off + i * 8);
        const u32 off = read_u32(cpu, tail_off + i * 8 + 4);
        if (type != 7 || size_t(off) + 8 > cpu.size()) {
            continue;
        }
        const u32 count = read_u32(cpu, off);
        const u32 list_off = read_u32(cpu, off + 4);
        if (count == 0 || count > 512 || size_t(list_off) + size_t(count) * 4 > cpu.size()) {
            continue;
        }
        const u32 decode_n = std::min(count, 24u);
        for (u32 t = 0; t < decode_n; ++t) {
            const u32 th_off = read_u32(cpu, list_off + t * 4);
            if (size_t(th_off) + 28 > cpu.size()) {
                continue;
            }
            TextureHeader hdr;
            if (!parse_texture_header(cpu.subspan(th_off, 28), hdr)) {
                continue;
            }
            try {
                mesh.textures.push_back(decode_xbox_texture(hdr, gpu));
            } catch (...) {
            }
        }
    }
}

}  // namespace

Mesh parse_model(const BundleEntry& entry) {
    Mesh mesh;
    mesh.name = entry.name;
    size_t gpu_base = 0;
    size_t gpu_limit = entry.gpu.size();
    if (!entry.gpu_chunks.empty()) {
        gpu_base = entry.gpu_chunks[0].offset;
        gpu_limit = gpu_base + entry.gpu_chunks[0].size;
        if (gpu_limit > entry.gpu.size()) {
            gpu_limit = entry.gpu.size();
        }
    }
    auto gpu_slice = (gpu_base < gpu_limit) ? entry.gpu.subspan(gpu_base, gpu_limit - gpu_base) : std::span<const u8>{};
    const auto streams = parse_vertex_streams(entry.cpu, gpu_slice);
    std::vector<u32> bases;
    bases.reserve(streams.size());
    for (const auto& s : streams) {
        bases.push_back(u32(mesh.positions.size()));
        mesh.positions.insert(mesh.positions.end(), s.positions.begin(), s.positions.end());
        mesh.uvs.resize(mesh.positions.size());
        for (size_t i = 0; i < s.uvs.size(); ++i) {
            mesh.uvs[bases.back() + i] = s.uvs[i];
        }
    }
    parse_push_buffers(entry.cpu, mesh, streams, bases);
    // Texture offsets are into the bundle GPU heap, not the first vertex chunk.
    parse_embedded_textures(entry.cpu, entry.gpu, mesh);
    if (mesh.indices.empty() && mesh.positions.size() >= 3) {
        const size_t n = (mesh.positions.size() / 3) * 3;
        mesh.indices.resize(n);
        for (size_t i = 0; i < n; ++i) {
            mesh.indices[i] = static_cast<u16>(i);
        }
    }
    return mesh;
}

std::vector<Mesh> extract_models(const Bundle& bundle) {
    std::vector<Mesh> meshes;
    for (const auto* e : entries_of_type(bundle, ResourceType::Model)) {
        auto m = parse_model(*e);
        if (!m.positions.empty()) {
            meshes.push_back(std::move(m));
        }
    }
    return meshes;
}

}  // namespace gbtg
