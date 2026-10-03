#include "gbtg/model.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace gbtg
{
namespace
{

struct VertStream
{
    size_t cpu_off = 0;
    u32 n = 0;
    std::vector<Vec3> positions;
    std::vector<Vec3> normals;
    std::vector<Vec2> uvs;
    std::vector<Vec2> uv1;
    std::vector<Vec2> uv2;
    std::vector<u16> bones;
};

std::vector<std::pair<u32, std::vector<Vec2>>> parse_uv_attrib(std::span<const u8> cpu, std::span<const u8> gpu,
                                                               u8 type)
{
    std::vector<std::pair<u32, std::vector<Vec2>>> uv_streams;
    for (size_t off = 0; off + 24 <= cpu.size(); ++off)
    {
        const u32 packed = read_u32(cpu, off);
        const u8 info_size = u8(packed & 0xFF);
        const u8 info_type = u8((packed >> 8) & 0xFF);
        if (info_type != type || info_size != 8)
        {
            continue;
        }
        const u32 goff = read_u32(cpu, off + 16);
        const u32 gsz = read_u32(cpu, off + 20);
        if (!gsz || gsz % 8 != 0 || goff + gsz > gpu.size())
        {
            continue;
        }
        const u32 n = gsz / 8;
        if (n > 200000)
        {
            continue;
        }
        std::vector<Vec2> uvs(n);
        for (u32 v = 0; v < n; ++v)
        {
            uvs[v].u = read_f32(gpu, goff + v * 8 + 0);
            uvs[v].v = read_f32(gpu, goff + v * 8 + 4);
        }
        uv_streams.emplace_back(n, std::move(uvs));
    }
    return uv_streams;
}

std::vector<VertStream> parse_vertex_streams(std::span<const u8> cpu, std::span<const u8> gpu)
{
    std::vector<VertStream> streams;
    if (cpu.size() < 24)
    {
        return streams;
    }
    for (size_t off = 0; off + 24 <= cpu.size(); ++off)
    {
        const u32 packed = read_u32(cpu, off);
        const u8 info_size = u8(packed & 0xFF);
        const u8 info_type = u8((packed >> 8) & 0xFF);
        if (info_type != 0x09 || info_size != 12)
        {
            continue;
        }
        const u32 goff = read_u32(cpu, off + 16);
        const u32 gsz = read_u32(cpu, off + 20);
        if (gsz < 12 || gsz % 12 != 0 || goff > gpu.size() || goff + gsz > gpu.size())
        {
            continue;
        }
        const u32 n = gsz / 12;
        if (n < 3 || n > 200000)
        {
            continue;
        }
        VertStream s;
        s.cpu_off = off;
        s.n = n;
        s.positions.resize(n);
        Vec3 bmin{1e9f, 1e9f, 1e9f};
        Vec3 bmax{-1e9f, -1e9f, -1e9f};
        for (u32 v = 0; v < n; ++v)
        {
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
        if (extent < 2.5f)
        {
            continue;
        }
        streams.push_back(std::move(s));
    }
    // Unit normals are type 0x0A, 12 bytes, one stream per vertex count.
    std::vector<std::pair<u32, std::vector<Vec3>>> normal_streams;
    for (size_t off = 0; off + 24 <= cpu.size(); ++off)
    {
        const u32 packed = read_u32(cpu, off);
        const u8 info_size = u8(packed & 0xFF);
        const u8 info_type = u8((packed >> 8) & 0xFF);
        if (info_type != 0x0A || info_size != 12)
        {
            continue;
        }
        const u32 goff = read_u32(cpu, off + 16);
        const u32 gsz = read_u32(cpu, off + 20);
        if (!gsz || gsz % 12 != 0 || goff + gsz > gpu.size())
        {
            continue;
        }
        const u32 n = gsz / 12;
        if (n > 200000)
        {
            continue;
        }
        std::vector<Vec3> normals(n);
        for (u32 v = 0; v < n; ++v)
        {
            normals[v] = Vec3{read_f32(gpu, goff + v * 12 + 0), read_f32(gpu, goff + v * 12 + 4),
                              read_f32(gpu, goff + v * 12 + 8)};
        }
        normal_streams.emplace_back(n, std::move(normals));
    }
    // Attract actors store positions in model space (the banjo arm is a hand). The
    // type-0 float4 that sits beside the position stream is not a bone index: its
    // values are around +/-80, and reading them as shader slots 24, 27, 30 scatters
    // vertices onto random joints. ndRigidSkinIdx is the palette, not this stream.
    std::vector<std::pair<u32, std::vector<u16>>> bone_streams;
    auto uv0 = parse_uv_attrib(cpu, gpu, 0x0D);
    auto uv1 = parse_uv_attrib(cpu, gpu, 0x0E);
    auto uv2 = parse_uv_attrib(cpu, gpu, 0x0F);
    for (auto& s : streams)
    {
        for (auto& uv : uv0)
        {
            if (uv.first == s.n && s.uvs.empty())
            {
                s.uvs = std::move(uv.second);
                uv.first = 0;
                break;
            }
        }
        for (auto& uv : uv1)
        {
            if (uv.first == s.n && s.uv1.empty())
            {
                s.uv1 = std::move(uv.second);
                uv.first = 0;
                break;
            }
        }
        for (auto& nrm : normal_streams)
        {
            if (nrm.first == s.n && s.normals.empty())
            {
                s.normals = std::move(nrm.second);
                nrm.first = 0;
                break;
            }
        }
        for (auto& uv : uv2)
        {
            if (uv.first == s.n && s.uv2.empty())
            {
                s.uv2 = std::move(uv.second);
                uv.first = 0;
                break;
            }
        }
        for (auto& bone : bone_streams)
        {
            if (bone.first == s.n && s.bones.empty())
            {
                s.bones = std::move(bone.second);
                bone.first = 0;
                break;
            }
        }
    }
    return streams;
}

void parse_skeleton(std::span<const u8> cpu, Mesh& mesh)
{
    constexpr u32 kTag = 0xCD01FFFF;
    size_t best = 0;
    int best_n = 0;
    for (size_t off = 0; off + 32 <= cpu.size(); off += 4)
    {
        if (read_u32(cpu, off) != kTag)
        {
            continue;
        }
        int n = 0;
        while (off + size_t(n) * 32 + 32 <= cpu.size() && read_u32(cpu, off + size_t(n) * 32) == kTag && n < 256)
        {
            ++n;
        }
        if (n > best_n)
        {
            best_n = n;
            best = off;
        }
    }
    if (best_n < 2)
    {
        return;
    }
    mesh.joints.resize(size_t(best_n));
    for (int i = 0; i < best_n; ++i)
    {
        const size_t rec = best + size_t(i) * 32;
        Joint& j = mesh.joints[size_t(i)];
        j.local = Vec3{read_f32(cpu, rec + 8), read_f32(cpu, rec + 12), read_f32(cpu, rec + 16)};
        j.world = Vec3{read_f32(cpu, rec + 20), read_f32(cpu, rec + 24), read_f32(cpu, rec + 28)};
        j.parent = -1;
    }
    // The dword at +4 is two u16 links. When the low word is this joint, the parent
    // is the joint whose high word points here. Otherwise the low word is the parent.
    std::vector<u16> link_lo;
    std::vector<u16> link_hi;
    link_lo.resize(size_t(best_n));
    link_hi.resize(size_t(best_n));
    for (int i = 0; i < best_n; ++i)
    {
        const size_t rec = best + size_t(i) * 32;
        link_lo[size_t(i)] = read_u16(cpu, rec + 4);
        link_hi[size_t(i)] = read_u16(cpu, rec + 6);
        if (link_lo[size_t(i)] != u16(i) && int(link_lo[size_t(i)]) < best_n)
        {
            mesh.joints[size_t(i)].parent = int(link_lo[size_t(i)]);
        }
    }
    for (int i = 0; i < best_n; ++i)
    {
        const int child = int(link_hi[size_t(i)]);
        if (child >= 0 && child < best_n && link_lo[size_t(child)] == u16(child))
        {
            mesh.joints[size_t(child)].parent = i;
        }
    }
}

float dist2(Vec3 a, Vec3 b)
{
    const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}

void emit_indices(Mesh& mesh, const std::vector<u16>& idx, u32 prim, u32 base, u32 stream_n)
{
    auto push = [&](u16 a, u16 b, u16 c)
    {
        if (a == b || b == c || a == c)
        {
            return;
        }
        if (a >= stream_n || b >= stream_n || c >= stream_n)
        {
            return;
        }
        const u32 ia = base + a, ib = base + b, ic = base + c;
        if (ia >= mesh.positions.size() || ib >= mesh.positions.size() || ic >= mesh.positions.size())
        {
            return;
        }
        const Vec3 pa = mesh.positions[ia], pb = mesh.positions[ib], pc = mesh.positions[ic];
        const Vec3 ab{pb.x - pa.x, pb.y - pa.y, pb.z - pa.z};
        const Vec3 ac{pc.x - pa.x, pc.y - pa.y, pc.z - pa.z};
        const Vec3 cr{ab.y * ac.z - ab.z * ac.y, ab.z * ac.x - ab.x * ac.z, ab.x * ac.y - ab.y * ac.x};
        const float cross2 = cr.x * cr.x + cr.y * cr.y + cr.z * cr.z;
        const float longest2 = std::max({dist2(pa, pb), dist2(pb, pc), dist2(pc, pa)});
        const float min_height = 0.005f * longest2;
        if (cross2 < 1e-12f || (longest2 > 1e-8f && cross2 < min_height * min_height))
        {
            return;
        }
        mesh.indices.push_back(u16(ia));
        mesh.indices.push_back(u16(ib));
        mesh.indices.push_back(u16(ic));
    };
    if (idx.size() < 3)
    {
        return;
    }
    // NV097_SET_BEGIN_END: 5=triangles, 6=strip, 7=fan. Unknown → strip unless count%3==0.
    if (prim == 5 || (prim != 6 && prim != 7 && idx.size() % 3 == 0))
    {
        for (size_t i = 0; i + 2 < idx.size(); i += 3)
        {
            push(idx[i], idx[i + 1], idx[i + 2]);
        }
        return;
    }
    if (prim == 7)
    {
        for (size_t i = 1; i + 1 < idx.size(); ++i)
        {
            push(idx[0], idx[i], idx[i + 1]);
        }
        return;
    }
    for (size_t i = 0; i + 2 < idx.size(); ++i)
    {
        if (i & 1)
        {
            push(idx[i + 1], idx[i], idx[i + 2]);
        }
        else
        {
            push(idx[i], idx[i + 1], idx[i + 2]);
        }
    }
}

void parse_scene_graph(std::span<const u8> cpu, Mesh& mesh, const std::vector<VertStream>& streams,
                       const std::vector<u32>& bases)
{
    if (streams.empty() || cpu.size() < 128)
    {
        return;
    }

    // project-grabbed: HeaderItem tree rooted at model+64+sizeof(UnknownStruct2).
    // Node payload offsets and ndShaderParam2 tables are relative to model+64.
    constexpr u32 kNodeBase = 64;
    constexpr u32 kHeaderItemSize = 40;
    constexpr u16 kNdShaderParam2 = 18;
    constexpr u16 kNdPushBuffer = 20;
    constexpr u16 kNdBGPushBuffer = 22;

    struct HeaderItem
    {
        u32 offset = 0;
        u16 type = 0;
        u32 next_child = 0;
        u32 next_sibling = 0;
        u32 un_other6 = 0;
    };

    auto read_item = [&](u32 rel) -> HeaderItem
    {
        HeaderItem hi;
        const size_t pos = size_t(rel) + 64;
        if (pos + kHeaderItemSize > cpu.size())
        {
            return hi;
        }
        hi.offset = read_u32(cpu, pos);
        hi.type = read_u16(cpu, pos + 4);
        hi.next_child = read_u32(cpu, pos + 20);
        hi.next_sibling = read_u32(cpu, pos + 24);
        hi.un_other6 = read_u32(cpu, pos + 32);
        return hi;
    };

    auto stream_at = [&](size_t pb_off) -> size_t
    {
        size_t found = 0;
        for (size_t i = 0; i < streams.size(); ++i)
        {
            if (streams[i].cpu_off <= pb_off)
            {
                found = i;
            }
        }
        return found;
    };

    auto read_cstring = [&](size_t abs_off) -> std::string
    {
        if (abs_off >= cpu.size())
        {
            return {};
        }
        size_t end = abs_off;
        while (end < cpu.size() && cpu[end] != 0 && end - abs_off < 64)
        {
            ++end;
        }
        return std::string(reinterpret_cast<const char*>(cpu.data() + abs_off), end - abs_off);
    };

    struct ActiveMaterial
    {
        i32 diffuse = -1;
        i32 secondary = -1; // colour1
        i32 detail = -1;
        i32 lightmap = -1;
        i32 celmap = -1;
    };

    // ndShaderParam2 material tables (project-grabbed read_shaderParam2).
    auto parse_param = [&](u32 un_other6) -> ActiveMaterial
    {
        ActiveMaterial mat;
        if (!un_other6)
        {
            return mat;
        }
        const size_t data = size_t(kNodeBase) + un_other6;
        if (data + 52 > cpu.size())
        {
            return mat;
        }
        const u32 tex_off = read_u32(cpu, data + 8);
        const u32 tex_count = read_u32(cpu, data + 12);
        const u32 name_off = read_u32(cpu, data + 36);
        const u32 name_count = read_u32(cpu, data + 40);
        if (tex_count == 0 || tex_count > 64 || name_count > 64 || !tex_off)
        {
            return mat;
        }
        std::vector<i32> refs;
        refs.reserve(tex_count);
        for (u32 m = 0; m < tex_count; ++m)
        {
            const size_t p = size_t(kNodeBase) + tex_off + size_t(m) * 28;
            if (p + 4 > cpu.size())
            {
                return {};
            }
            refs.push_back(i32(read_u32(cpu, p)));
        }
        for (u32 m = 0; m < name_count; ++m)
        {
            const size_t p = size_t(kNodeBase) + name_off + size_t(m) * 16;
            if (p + 16 > cpu.size())
            {
                break;
            }
            const u32 str_off = read_u32(cpu, p);
            const u32 unk1 = read_u32(cpu, p + 4);
            const u32 index = read_u32(cpu, p + 8);
            const i32 unk3 = i32(read_u32(cpu, p + 12));
            if (unk1 != 0 || unk3 != -1 || index >= refs.size())
            {
                continue;
            }
            const std::string name = read_cstring(size_t(kNodeBase) + str_off);
            if (name == "colour0")
            {
                mat.diffuse = refs[index];
            }
            else if (name == "colour1")
            {
                mat.secondary = refs[index];
            }
            else if (name == "colour2" && mat.detail < 0)
            {
                mat.detail = refs[index];
            }
            else if (name == "colour3" && mat.detail < 0)
            {
                mat.detail = refs[index];
            }
            else if (name == "RARE_lightmap0" || name == "RARE_lightmap-1")
            {
                mat.lightmap = refs[index];
            }
            else if (name == "RARE_celmap0")
            {
                mat.celmap = refs[index];
            }
        }
        return mat;
    };

    auto emit_push = [&](size_t tag, const ActiveMaterial& mat, const std::vector<u8>& skin)
    {
        if (tag + 28 > cpu.size())
        {
            return;
        }
        const u32 count = read_u32(cpu, tag + 16);
        const u32 this_ptr = read_u32(cpu, tag + 20);
        const u32 prim = read_u32(cpu, tag + 24);
        if (this_ptr != tag - 36 || (prim != 5 && prim != 6 && prim != 7))
        {
            return;
        }
        const size_t idx_off = tag + 28;
        if (count < 3 || count >= 500000 || idx_off + count * 2 > cpu.size())
        {
            return;
        }
        std::vector<u16> idx;
        idx.reserve(count);
        for (u32 k = 0; k < count; ++k)
        {
            idx.push_back(read_u16(cpu, idx_off + k * 2));
        }
        const size_t si = stream_at(tag);
        const size_t start = mesh.indices.size();
        emit_indices(mesh, idx, prim, bases[si], streams[si].n);
        const u32 added = u32(mesh.indices.size() - start);
        if (!added)
        {
            return;
        }
        Submesh sm;
        sm.first_index = u32(start);
        sm.index_count = added;
        sm.texture = mat.diffuse;
        sm.layer1 = mat.secondary;
        sm.detail = mat.detail;
        sm.lightmap = mat.lightmap;
        sm.celmap = mat.celmap;
        sm.skin = skin;
        mesh.submeshes.push_back(sm);
    };

    // ndRigidSkinIdx (type 11). un_other6 is a byte remap, relative to model+64.
    // Consecutive nodes are windows into one stream, so the length is the gap to the next window.
    std::vector<u32> skin_offs;
    {
        std::vector<u32> scan{52};
        std::vector<u8> scan_seen(cpu.size() / 4 + 1, 0);
        while (!scan.empty())
        {
            const u32 rel = scan.back();
            scan.pop_back();
            if (rel == 0 || size_t(rel) / 4 >= scan_seen.size() || scan_seen[rel / 4])
            {
                continue;
            }
            scan_seen[rel / 4] = 1;
            const HeaderItem hi = read_item(rel);
            if (hi.next_sibling)
            {
                scan.push_back(hi.next_sibling);
            }
            if (hi.next_child)
            {
                scan.push_back(hi.next_child);
            }
            if (hi.type == 11)
            {
                skin_offs.push_back(hi.un_other6);
            }
        }
        std::sort(skin_offs.begin(), skin_offs.end());
        skin_offs.erase(std::unique(skin_offs.begin(), skin_offs.end()), skin_offs.end());
    }

    ActiveMaterial active;
    std::vector<u8> active_skin;
    std::vector<u32> stack{52}; // sizeof(UnknownStruct2) — project-grabbed tree root
    std::vector<u8> seen(cpu.size() / 4 + 1, 0);
    while (!stack.empty())
    {
        const u32 rel = stack.back();
        stack.pop_back();
        if (rel == 0 || size_t(rel) / 4 >= seen.size() || seen[rel / 4])
        {
            continue;
        }
        seen[rel / 4] = 1;
        const HeaderItem hi = read_item(rel);
        if (hi.type == 0 && hi.offset == 0)
        {
            continue;
        }
        if (hi.next_sibling)
        {
            stack.push_back(hi.next_sibling);
        }
        if (hi.next_child)
        {
            stack.push_back(hi.next_child);
        }
        if (hi.type == kNdShaderParam2)
        {
            active = parse_param(hi.un_other6);
        }
        else if (hi.type == 11)
        {
            active_skin.clear();
            const auto it = std::lower_bound(skin_offs.begin(), skin_offs.end(), hi.un_other6);
            u32 len = 16;
            if (it != skin_offs.end() && std::next(it) != skin_offs.end() && *std::next(it) > *it)
            {
                len = *std::next(it) - *it;
            }
            len = std::min(len, 64u);
            const size_t p = size_t(kNodeBase) + hi.un_other6;
            if (p < cpu.size())
            {
                len = std::min(len, u32(cpu.size() - p));
                active_skin.assign(cpu.begin() + p, cpu.begin() + p + len);
            }
        }
        else if (hi.type == kNdPushBuffer || hi.type == kNdBGPushBuffer)
        {
            // un_other6 == 1: single strip/list immediately after the tag.
            if (hi.un_other6 == 1)
            {
                const size_t tag = size_t(kNodeBase) + hi.offset;
                emit_push(tag, active, active_skin);
            }
        }
    }
}

struct GpuMap
{
    u32 abs_off = 0;
    i32 chunk = -1;
};

GpuMap map_gpu(std::span<const ResourceSubchunk> chunks, u32 logical)
{
    if (chunks.empty())
    {
        return {logical, -1};
    }
    u64 cursor = 0;
    for (i32 i = 0; i < i32(chunks.size()); ++i)
    {
        const auto& c = chunks[i];
        if (u64(logical) < cursor + c.size)
        {
            return {u32(u64(c.offset) + (u64(logical) - cursor)), i};
        }
        cursor += c.size;
    }
    return {logical, -1};
}

void parse_embedded_textures(std::span<const u8> cpu, std::span<const u8> gpu, std::span<const ResourceSubchunk> chunks,
                             Mesh& mesh, std::vector<i32>& tex_chunks)
{
    if (cpu.size() < 32)
    {
        return;
    }
    const u32 tail_off = read_u32(cpu, 0);
    const u32 tail_count = read_u32(cpu, 4);
    if (tail_count == 0 || tail_count > 256 || size_t(tail_off) + size_t(tail_count) * 8 > cpu.size())
    {
        return;
    }
    for (u32 i = 0; i < tail_count; ++i)
    {
        const u32 type = read_u32(cpu, tail_off + i * 8);
        const u32 off = read_u32(cpu, tail_off + i * 8 + 4);
        if (type != 7 || size_t(off) + 8 > cpu.size())
        {
            continue;
        }
        const u32 count = read_u32(cpu, off);
        const u32 list_off = read_u32(cpu, off + 4);
        if (count == 0 || count > 512 || size_t(list_off) + size_t(count) * 4 > cpu.size())
        {
            continue;
        }
        mesh.textures.resize(count);
        tex_chunks.assign(count, -1);
        for (u32 t = 0; t < count; ++t)
        {
            const u32 th_off = read_u32(cpu, list_off + t * 4);
            if (size_t(th_off) + 28 > cpu.size())
            {
                continue;
            }
            TextureHeader hdr;
            if (!parse_texture_header(cpu.subspan(th_off, 28), hdr))
            {
                continue;
            }
            const GpuMap mapped = map_gpu(chunks, hdr.texture_offset);
            hdr.texture_offset = mapped.abs_off;
            tex_chunks[t] = mapped.chunk;
            try
            {
                mesh.textures[t] = decode_xbox_texture(hdr, gpu);
            }
            catch (...)
            {
            }
        }
    }
}

struct TexStats
{
    float mean = 0;
    float black = 0;
    float chroma = 0;
    float contrast = 0;
    float center = 0;
    float corners = 0;
    float wrap = 0;
};

TexStats analyze_tex(const RgbaImage& t)
{
    TexStats s;
    if (t.bgra.empty() || t.width <= 0 || t.height <= 0)
    {
        return s;
    }
    const int stepx = std::max(1, t.width / 32);
    const int stepy = std::max(1, t.height / 32);
    long chroma = 0, contrast = 0, lum_sum = 0;
    int n = 0, cn = 0, black = 0;
    for (int y = 0; y < t.height; y += stepy)
    {
        for (int x = 0; x < t.width; x += stepx)
        {
            const u8* p = t.bgra.data() + (size_t(y) * t.width + x) * 4;
            const int lum = int(p[0]) + int(p[1]) + int(p[2]);
            lum_sum += lum;
            chroma += std::abs(int(p[2]) - p[1]) + std::abs(int(p[1]) - p[0]) + std::abs(int(p[0]) - p[2]);
            ++n;
            if (lum < 24)
            {
                ++black;
            }
            if (x + stepx < t.width)
            {
                const u8* q = t.bgra.data() + (size_t(y) * t.width + x + stepx) * 4;
                contrast += std::abs(int(p[0]) - q[0]) + std::abs(int(p[1]) - q[1]) + std::abs(int(p[2]) - q[2]);
                ++cn;
            }
        }
    }
    auto sample = [&](int x, int y)
    {
        const u8* p = t.bgra.data() + (size_t(y) * t.width + x) * 4;
        return float(int(p[0]) + int(p[1]) + int(p[2]));
    };
    s.mean = n ? float(lum_sum) / float(n) : 0.f;
    s.black = n ? float(black) / float(n) : 1.f;
    s.chroma = n ? float(chroma) / float(n) : 0.f;
    s.contrast = cn ? float(contrast) / float(cn) : 0.f;
    s.center = sample(t.width / 2, t.height / 2);
    s.corners =
        0.25f * (sample(0, 0) + sample(t.width - 1, 0) + sample(0, t.height - 1) + sample(t.width - 1, t.height - 1));
    long wrap = 0;
    int wn = 0;
    for (int y = 0; y < t.height; y += stepy)
    {
        const u8* l = t.bgra.data() + (size_t(y) * t.width) * 4;
        const u8* r = t.bgra.data() + (size_t(y) * t.width + (t.width - 1)) * 4;
        wrap += std::abs(int(l[0]) - r[0]) + std::abs(int(l[1]) - r[1]) + std::abs(int(l[2]) - r[2]);
        ++wn;
    }
    for (int x = 0; x < t.width; x += stepx)
    {
        const u8* a = t.bgra.data() + x * 4;
        const u8* b = t.bgra.data() + (size_t(t.height - 1) * t.width + x) * 4;
        wrap += std::abs(int(a[0]) - b[0]) + std::abs(int(a[1]) - b[1]) + std::abs(int(a[2]) - b[2]);
        ++wn;
    }
    s.wrap = wn ? float(wrap) / float(wn) : 0.f;
    return s;
}

bool is_ramp_tex(const RgbaImage& t)
{
    return t.bgra.empty() || t.width <= 4 || t.height <= 4;
}

bool is_lightmap_tex(const TexStats& s)
{
    return (s.black > 0.15f && s.corners < 50.f) || (s.black > 0.40f);
}

bool is_cookie_tex(const TexStats& s)
{
    // Soft radial glow / spotlight cookie: black frame, bright center.
    return s.black > 0.75f && s.center > s.corners * 3.f + 40.f;
}

bool is_dead_albedo(const TexStats& s)
{
    return s.mean < 20.f || s.black > 0.90f;
}

bool looks_like_light_layer(const TexStats& s)
{
    // Baked lighting atlases: lots of black, bright hotspots, low chroma.
    return s.black > 0.25f && s.contrast > 8.f && s.chroma < 25.f;
}

bool is_illustration_tex(const RgbaImage& t, const TexStats& s)
{
    if (is_ramp_tex(t) || is_lightmap_tex(s))
    {
        return false;
    }
    return s.chroma >= 48.f && std::min(t.width, t.height) >= 64;
}

bool is_unique_atlas(const RgbaImage& t, const TexStats& s)
{
    if (is_ramp_tex(t) || is_lightmap_tex(s) || is_illustration_tex(t, s))
    {
        return false;
    }
    if (t.width >= 512 && t.height >= 512)
    {
        return true;
    }
    return std::min(t.width, t.height) >= 256 && s.wrap > 40.f;
}

float submesh_uv_span(const Mesh& mesh, const Submesh& sm, const std::vector<Vec2>& uvs)
{
    if (uvs.empty() || sm.index_count < 3)
    {
        return 0.f;
    }
    float umin = 1e9f, umax = -1e9f, vmin = 1e9f, vmax = -1e9f;
    int n = 0;
    const size_t end = size_t(sm.first_index) + sm.index_count;
    for (size_t i = sm.first_index; i < end && i < mesh.indices.size(); ++i)
    {
        const u16 vi = mesh.indices[i];
        if (vi >= uvs.size())
        {
            continue;
        }
        umin = std::min(umin, uvs[vi].u);
        umax = std::max(umax, uvs[vi].u);
        vmin = std::min(vmin, uvs[vi].v);
        vmax = std::max(vmax, uvs[vi].v);
        ++n;
    }
    return n ? (umax - umin) + (vmax - vmin) : 0.f;
}

void choose_uv_channels(Mesh& mesh, const std::vector<TexStats>& stats)
{
    const bool actor = mesh.name.find("actor") != std::string::npos;
    for (auto& sm : mesh.submeshes)
    {
        const float s0 = submesh_uv_span(mesh, sm, mesh.uvs);
        const float s1 = submesh_uv_span(mesh, sm, mesh.uv1);
        // Actors keep diffuse on UV0. Env tiling surfaces often store diffuse in UV1.
        if (!actor && s1 > s0 * 1.15f && s1 > 1.15f)
        {
            sm.albedo_uv = 1;
            sm.lightmap_uv = 0;
        }
        else
        {
            sm.albedo_uv = 0;
            sm.lightmap_uv = (s1 > 0.05f) ? 1 : 0;
        }
        if (sm.lightmap >= 0 && sm.lightmap_uv == sm.albedo_uv)
        {
            sm.lightmap = -1;
        }
        if (sm.texture >= 0 && size_t(sm.texture) < stats.size() &&
            sm.texture < i32(mesh.textures.size()) &&
            (is_illustration_tex(mesh.textures[sm.texture], stats[sm.texture]) ||
             is_unique_atlas(mesh.textures[sm.texture], stats[sm.texture])))
        {
            sm.albedo_uv = 0;
            if (sm.lightmap_uv == 0)
            {
                sm.lightmap = -1;
            }
        }
    }
}

} // namespace

Mesh parse_model(const BundleEntry& entry)
{
    Mesh mesh;
    mesh.name = entry.name;
    size_t gpu_base = 0;
    size_t gpu_limit = entry.gpu.size();
    if (!entry.gpu_chunks.empty())
    {
        gpu_base = entry.gpu_chunks[0].offset;
        gpu_limit = gpu_base + entry.gpu_chunks[0].size;
        if (gpu_limit > entry.gpu.size())
        {
            gpu_limit = entry.gpu.size();
        }
    }
    auto gpu_slice = (gpu_base < gpu_limit) ? entry.gpu.subspan(gpu_base, gpu_limit - gpu_base) : std::span<const u8>{};
    const auto streams = parse_vertex_streams(entry.cpu, gpu_slice);
    bool any_bones = false;
    for (const auto& s : streams)
    {
        if (!s.bones.empty())
        {
            any_bones = true;
        }
    }
    std::vector<u32> bases;
    bases.reserve(streams.size());
    for (const auto& s : streams)
    {
        bases.push_back(u32(mesh.positions.size()));
        mesh.positions.insert(mesh.positions.end(), s.positions.begin(), s.positions.end());
        mesh.normals.resize(mesh.positions.size());
        for (size_t i = 0; i < s.normals.size(); ++i)
        {
            mesh.normals[bases.back() + i] = s.normals[i];
        }
        mesh.uvs.resize(mesh.positions.size());
        mesh.uv1.resize(mesh.positions.size());
        mesh.uv2.resize(mesh.positions.size());
        for (size_t i = 0; i < s.uvs.size(); ++i)
        {
            mesh.uvs[bases.back() + i] = s.uvs[i];
        }
        for (size_t i = 0; i < s.uv1.size(); ++i)
        {
            mesh.uv1[bases.back() + i] = s.uv1[i];
        }
        for (size_t i = 0; i < s.uv2.size(); ++i)
        {
            mesh.uv2[bases.back() + i] = s.uv2[i];
        }
        if (any_bones)
        {
            mesh.bones.resize(mesh.positions.size(), 0);
            for (size_t i = 0; i < s.bones.size(); ++i)
            {
                mesh.bones[bases.back() + i] = s.bones[i];
            }
        }
    }
    parse_skeleton(entry.cpu, mesh);
    parse_scene_graph(entry.cpu, mesh, streams, bases);
    std::vector<i32> tex_chunks;
    parse_embedded_textures(entry.cpu, entry.gpu, entry.gpu_chunks, mesh, tex_chunks);
    (void)tex_chunks;
    std::vector<TexStats> stats(mesh.textures.size());
    for (size_t i = 0; i < mesh.textures.size(); ++i)
    {
        stats[i] = analyze_tex(mesh.textures[i]);
    }
    // Resolve layered env materials: colour0 can be a near-black base while
    // colour2 holds the visible surface and colour1 holds baked lighting.
    for (auto& sm : mesh.submeshes)
    {
        auto valid_surface = [&](i32 i) -> bool
        {
            return i >= 0 && size_t(i) < mesh.textures.size() && !mesh.textures[i].bgra.empty() &&
                   mesh.textures[i].width > 4 && mesh.textures[i].height > 4;
        };
        auto alive = [&](i32 i) -> bool
        {
            return valid_surface(i) && !is_dead_albedo(stats[size_t(i)]);
        };

        if (!valid_surface(sm.texture))
        {
            sm.texture = -1;
        }
        if (!valid_surface(sm.detail))
        {
            sm.detail = -1;
        }
        if (!valid_surface(sm.layer1))
        {
            sm.layer1 = -1;
        }
        if (sm.celmap >= 0 &&
            (size_t(sm.celmap) >= mesh.textures.size() || mesh.textures[sm.celmap].bgra.empty()))
        {
            sm.celmap = -1;
        }

        // Promote a living colour2/colour1 when colour0 is empty or near-black.
        // Some env bases sit just above the dead-albedo cutoff while colour2 holds the surface.
        const bool dark_base = sm.texture >= 0 && stats[size_t(sm.texture)].mean < 80.f;
        const bool detail_is_surface =
            sm.texture >= 0 && alive(sm.detail) && stats[size_t(sm.detail)].mean > stats[size_t(sm.texture)].mean * 2.5f;
        if (sm.texture < 0 || (sm.texture >= 0 && is_dead_albedo(stats[size_t(sm.texture)])) ||
            (dark_base && detail_is_surface))
        {
            if (alive(sm.detail))
            {
                sm.texture = sm.detail;
                sm.detail = -1;
            }
            else if (alive(sm.layer1) && !looks_like_light_layer(stats[size_t(sm.layer1)]))
            {
                sm.texture = sm.layer1;
                sm.layer1 = -1;
            }
            else if (sm.texture >= 0 && is_dead_albedo(stats[size_t(sm.texture)]))
            {
                sm.texture = -1;
            }
        }

        // colour1 often carries the baked light atlas when RARE_lightmap is absent.
        if (sm.lightmap < 0 && sm.layer1 >= 0 && looks_like_light_layer(stats[size_t(sm.layer1)]))
        {
            sm.lightmap = sm.layer1;
            sm.layer1 = -1;
        }

        if (sm.lightmap >= 0 && size_t(sm.lightmap) < mesh.textures.size())
        {
            if (mesh.textures[sm.lightmap].bgra.empty() || is_cookie_tex(stats[size_t(sm.lightmap)]))
            {
                sm.lightmap = -1;
            }
        }
        else if (sm.lightmap >= 0)
        {
            sm.lightmap = -1;
        }

        // Detail multiply against a second surface map; drop if missing.
        if (sm.detail >= 0 && sm.detail == sm.texture)
        {
            sm.detail = -1;
        }
    }
    choose_uv_channels(mesh, stats);
    if (mesh.indices.empty() && mesh.positions.size() >= 3)
    {
        const size_t n = (mesh.positions.size() / 3) * 3;
        mesh.indices.resize(n);
        for (size_t i = 0; i < n; ++i)
        {
            mesh.indices[i] = static_cast<u16>(i);
        }
    }
    return mesh;
}

std::vector<Mesh> extract_models(const Bundle& bundle)
{
    std::vector<Mesh> meshes;
    for (const auto* e : entries_of_type(bundle, ResourceType::Model))
    {
        auto m = parse_model(*e);
        if (!m.positions.empty())
        {
            meshes.push_back(std::move(m));
        }
    }
    return meshes;
}

} // namespace gbtg
