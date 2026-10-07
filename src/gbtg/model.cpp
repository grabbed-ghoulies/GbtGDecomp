#include "gbtg/model.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <unordered_map>
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
    std::vector<u32> diffuse;
    std::vector<u32> specular;
    std::vector<Vec2> uvs;
    std::vector<Vec2> uv1;
    std::vector<Vec2> uv2;
    std::vector<Vec2> uv3;
    std::vector<u16> bones;
    std::vector<u16> bone2;
    std::vector<u16> bone3;
    std::vector<u16> bone4;
    std::vector<float> weight0;
    std::vector<float> weight1;
    std::vector<float> weight2;
    std::vector<float> weight3;
    std::vector<u8> attribute_types;
    std::vector<std::string> issues;
};

// Xbox 0x00158430 publishes the descriptors of ndVertexBuffer (type 19).
// Xbox 0x00157A50 dispatches ndBlendShape (types 9 and 23); 0x00157880
// publishes its primary descriptor array. Morph target evaluation remains separate.
// Attribute ownership comes from those arrays, never from a byte-pattern scan.
std::vector<VertStream> parse_vertex_streams(std::span<const u8> cpu, std::span<const u8> gpu)
{
    std::vector<VertStream> streams;
    std::vector<u32> pending{52};
    std::vector<u32> visited;
    while (!pending.empty())
    {
        const u32 rel = pending.back();
        pending.pop_back();
        const size_t node = size_t(rel) + 64;
        if (!rel || node + 40 > cpu.size() || std::find(visited.begin(), visited.end(), rel) != visited.end())
        {
            continue;
        }
        visited.push_back(rel);
        pending.push_back(read_u32(cpu, node + 24));
        pending.push_back(read_u32(cpu, node + 20));
        const u16 type = read_u16(cpu, node + 4);
        size_t buffer = node;
        if (type == 9 || type == 23)
        {
            const size_t list = 64 + size_t(read_u32(cpu, node + 32));
            if (list + 4 > cpu.size() || read_u16(cpu, node + 36) == 0)
            {
                throw Error("ndBlendShape primary buffer out of range");
            }
            buffer = 64 + size_t(read_u32(cpu, list));
        }
        else if (type != 19)
        {
            continue;
        }
        if (buffer + 40 > cpu.size())
        {
            throw Error("vertex buffer header out of range");
        }
        const size_t attributes = 64 + size_t(read_u32(cpu, buffer + 32));
        const u32 count = read_u32(cpu, buffer + 36);
        if (attributes > cpu.size() || size_t(count) > (cpu.size() - attributes) / 24)
        {
            throw Error("vertex descriptor array out of range");
        }
        VertStream stream;
        stream.cpu_off = node;
        for (u32 k = 0; k < count; ++k)
        {
            const size_t d = attributes + size_t(k) * 24;
            stream.attribute_types.push_back(cpu[d + 1]);
            if (cpu[d + 1] == 9 && cpu[d] != 0)
            {
                stream.n = read_u32(cpu, d + 20) / cpu[d];
            }
        }
        for (u32 k = 0; k < count; ++k)
        {
            const size_t d = attributes + size_t(k) * 24;
            const u8 stride = cpu[d];
            const u8 attr = cpu[d + 1];
            const size_t offset = read_u32(cpu, d + 16);
            const size_t bytes = read_u32(cpu, d + 20);
            if (!bytes)
            {
                continue;
            }
            if (!stride || bytes % stride || offset > gpu.size() || bytes > gpu.size() - offset ||
                bytes / stride != stream.n)
            {
                throw Error("vertex attribute size/range mismatch");
            }
            const bool supported = ((attr == 9 || attr == 10) && stride == 12) ||
                ((attr == 11 || attr == 12) && stride == 4) ||
                (attr >= 13 && attr <= 16 && stride == 8) ||
                ((attr == 0 || attr == 8) && (stride == 4 || stride == 8 || stride == 16));
            if (!supported && (attr < 27 || attr > 34))
            {
                stream.issues.push_back("untranslated vertex attribute " + std::to_string(attr) +
                                        " stride " + std::to_string(stride));
            }
            for (u32 v = 0; v < stream.n; ++v)
            {
                const size_t q = offset + size_t(v) * stride;
                if ((attr == 9 || attr == 10) && stride == 12)
                {
                    const Vec3 value{read_f32(gpu, q), read_f32(gpu, q + 4), read_f32(gpu, q + 8)};
                    (attr == 9 ? stream.positions : stream.normals).push_back(value);
                }
                else if (attr >= 13 && attr <= 16 && stride == 8)
                {
                    auto& uv = attr == 13 ? stream.uvs : (attr == 14 ? stream.uv1 :
                        (attr == 15 ? stream.uv2 : stream.uv3));
                    uv.push_back(Vec2{read_f32(gpu, q), read_f32(gpu, q + 4)});
                }
                else if ((attr == 11 || attr == 12) && stride == 4)
                {
                    (attr == 11 ? stream.diffuse : stream.specular).push_back(read_u32(gpu, q));
                }
                else if (attr == 0 && (stride == 4 || stride == 8 || stride == 16))
                {
                    const float first = read_f32(gpu, q);
                    const float second = stride >= 8 ? read_f32(gpu, q + 4) : 0.f;
                    stream.bones.push_back(u16((first - 24.f) / 3.f));
                    stream.bone2.push_back(second == 0.f ? 0 : u16((second - 24.f) / 3.f));
                    const float third = stride == 16 ? read_f32(gpu, q + 8) : 0.f;
                    const float fourth = stride == 16 ? read_f32(gpu, q + 12) : 0.f;
                    stream.bone3.push_back(third == 0.f ? 0 : u16((third - 24.f) / 3.f));
                    stream.bone4.push_back(fourth == 0.f ? 0 : u16((fourth - 24.f) / 3.f));
                }
                else if (attr == 8 && (stride == 4 || stride == 8 || stride == 16))
                {
                    stream.weight0.push_back(read_f32(gpu, q));
                    stream.weight1.push_back(stride >= 8 ? read_f32(gpu, q + 4) : 0.f);
                    stream.weight2.push_back(stride == 16 ? read_f32(gpu, q + 8) : 0.f);
                    stream.weight3.push_back(stride == 16 ? read_f32(gpu, q + 12) : 0.f);
                }
            }
        }
        if (!stream.positions.empty())
        {
            streams.push_back(std::move(stream));
        }
    }
    return streams;
}

void parse_skeleton(std::span<const u8> cpu, Mesh& mesh)
{
    // Xbox FUN_00156280 finds ndSkeleton by HeaderItem type. The type id is the
    // index of "ndSkeleton" in the 0x54-byte name table (type 2). FUN_00030c70
    // then reads the joint count from the node at +0x20 and the joint array from
    // the node at +0x24. Both are file offsets relative to model+64 until load
    // relocates them. Each joint is 0x20 bytes. FUN_0002fac0 uses the first u16
    // as the parent (0xFFFF selects DAT_0042a7c0, the identity) and the floats
    // at +4 and +16 as the local and bind translations.
    constexpr u32 kNodeBase = 64;
    constexpr u16 kNdSkeleton = 2;
    if (cpu.size() < 128)
    {
        return;
    }
    std::vector<u32> stack{52};
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
        const size_t pos = size_t(rel) + kNodeBase;
        if (pos + 40 > cpu.size())
        {
            continue;
        }
        const u16 type = read_u16(cpu, pos + 4);
        const u32 child = read_u32(cpu, pos + 20);
        const u32 sibling = read_u32(cpu, pos + 24);
        if (child)
        {
            stack.push_back(child);
        }
        if (sibling)
        {
            stack.push_back(sibling);
        }
        if (type != kNdSkeleton)
        {
            continue;
        }
        const u32 count = read_u32(cpu, pos + 32);
        const u32 data = read_u32(cpu, pos + 36);
        if (count < 2 || count > 256)
        {
            continue;
        }
        const size_t base = size_t(kNodeBase) + data;
        if (base + size_t(count) * 32 > cpu.size())
        {
            continue;
        }
        mesh.joints.resize(count);
        for (u32 i = 0; i < count; ++i)
        {
            const size_t rec = base + size_t(i) * 32;
            Joint& j = mesh.joints[i];
            const u16 parent = read_u16(cpu, rec);
            j.parent = parent == 0xFFFF ? -1 : int(parent);
            j.local = Vec3{read_f32(cpu, rec + 4), read_f32(cpu, rec + 8), read_f32(cpu, rec + 12)};
            j.world = Vec3{read_f32(cpu, rec + 16), read_f32(cpu, rec + 20), read_f32(cpu, rec + 24)};
        }
        return;
    }
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
            throw Error("model index outside declared vertex stream");
        }
        const u32 ia = base + a, ib = base + b, ic = base + c;
        if (ia >= mesh.positions.size() || ib >= mesh.positions.size() || ic >= mesh.positions.size())
        {
            throw Error("model index outside concatenated vertex buffer");
        }
        mesh.indices.push_back(ia);
        mesh.indices.push_back(ib);
        mesh.indices.push_back(ic);
    };
    if (idx.size() < 3)
    {
        return;
    }
    // NV097_SET_BEGIN_END: 5=triangles, 6=strip, 7=fan.
    if (prim == 5)
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
    if (prim != 6)
    {
        mesh.issues.push_back("unsupported primitive " + std::to_string(prim));
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
        u32 count = 0; // header +36. ndRigidSkinIdx stores the palette length here.
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
        hi.count = read_u32(cpu, pos + 36);
        return hi;
    };

    size_t active_stream = 0;
    std::string active_shader_name;

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

    // Xbox 0x001568E0: root+0x10 is the name table, with 12-byte entries.
    // Preserve the shader parameter names used by the transition page bindings.
    std::unordered_map<u32, std::string> node_names;
    const u32 names_rel = read_u32(cpu, kNodeBase + 0x10);
    if (names_rel != 0)
    {
        const size_t names = kNodeBase + size_t(names_rel);
        const u32 count = read_u32(cpu, names);
        const size_t list = kNodeBase + size_t(read_u32(cpu, names + 4));
        for (u32 i = 0; i < count; ++i)
        {
            const size_t entry = list + size_t(i) * 12;
            if (read_u32(cpu, entry + 8) == 4)
            {
                const size_t object = kNodeBase + size_t(read_u32(cpu, entry + 4));
                node_names[read_u32(cpu, object)] = read_cstring(kNodeBase + size_t(read_u32(cpu, entry)));
            }
        }
    }

    struct ActiveMaterial
    {
        i32 diffuse = -1;
        i32 colour0_stage = -1;
        i32 secondary = -1; // colour1
        i32 detail = -1;
        i32 lightmap = -1;
        i32 celmap = -1;
        Vec3 color{1.f, 1.f, 1.f};
        u8 alpha_ref = 0;
        std::array<i32, 4> stages{-1, -1, -1, -1};
        std::array<u8, 4> address_u{1, 1, 1, 1};
        std::array<u8, 4> address_v{1, 1, 1, 1};
        std::vector<u32> constants;
    };
    std::array<u32, 60> active_pixel_shader{};
    bool has_pixel_shader = false;
    i32 render_mode = -1;
    std::array<u8, 2> colour_source{255, 255};
    u8 skin_weight_mask = 0;
    bool uses_skin_palette = false;
    std::array<u8, 4> stage_uv{};
    bool direct_uv = false;

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
        // Xbox 0x0015AD20. The count at +20 is how many RGBA bytes at +0 are
        // uploaded, register 0 first, each byte times 1/255. Byte +24 is the
        // alpha-test ref: 0 turns the test off, any other value is D3DCMP_GREATER.
        const u32 col_off = read_u32(cpu, data);
        const u32 col_count = read_u32(cpu, data + 20);
        if (col_count > 0 && col_count <= 8)
        {
            const size_t p = size_t(kNodeBase) + col_off;
            if (p + 3 < cpu.size())
            {
                mat.color = Vec3{cpu[p] / 255.f, cpu[p + 1] / 255.f, cpu[p + 2] / 255.f};
            }
            if (p + size_t(col_count) * 4 <= cpu.size())
            {
                for (u32 i = 0; i < col_count; ++i)
                {
                    // Material bytes are RGBA; shader literals are ARGB.
                    const size_t q = p + size_t(i) * 4;
                    mat.constants.push_back((u32(cpu[q + 3]) << 24) | (u32(cpu[q]) << 16) |
                                            (u32(cpu[q + 1]) << 8) | cpu[q + 2]);
                }
            }
        }
        mat.alpha_ref = cpu[data + 24];
        if (tex_count == 0 || tex_count > 64 || name_count > 64 || !tex_off)
        {
            return mat;
        }
        std::vector<i32> refs;
        refs.reserve(tex_count);
        for (u32 m = 0; m < tex_count; ++m)
        {
            const size_t p = size_t(kNodeBase) + tex_off + size_t(m) * 28;
            if (p + 28 > cpu.size())
            {
                return {};
            }
            refs.push_back(i32(read_u32(cpu, p)));
            if (m < 4)
            {
                mat.stages[m] = refs.back();
                mat.address_u[m] = cpu[p + 4];
                mat.address_v[m] = cpu[p + 5];
            }
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
                mat.colour0_stage = i32(index);
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

    auto emit_indexed = [&](size_t node, size_t idx_off, u32 count, u32 prim, const ActiveMaterial& mat,
                            const std::vector<u8>& skin, const std::vector<u16>& mtx)
    {
        if (prim != 5 && prim != 6 && prim != 7)
        {
            mesh.issues.push_back("unsupported primitive " + std::to_string(prim));
            return;
        }
        if (idx_off > cpu.size() || size_t(count) > (cpu.size() - idx_off) / 2)
        {
            throw Error("model index array out of range");
        }
        if (count < 3)
        {
            return;
        }
        std::vector<u16> idx;
        idx.reserve(count);
        for (u32 k = 0; k < count; ++k)
        {
            idx.push_back(read_u16(cpu, idx_off + k * 2));
        }
        const size_t si = active_stream;
        const size_t start = mesh.indices.size();
        emit_indices(mesh, idx, prim, bases[si], streams[si].n);
        const u32 added = u32(mesh.indices.size() - start);
        if (!added)
        {
            return;
        }
        Submesh sm;
        sm.shader_name = active_shader_name;
        sm.first_index = u32(start);
        sm.index_count = added;
        sm.texture = mat.diffuse;
        sm.colour0_stage = mat.colour0_stage;
        sm.layer1 = mat.secondary;
        sm.detail = mat.detail;
        sm.lightmap = mat.lightmap;
        sm.celmap = mat.celmap;
        sm.skin = skin;
        sm.mtx = mtx;
        sm.color = mat.color;
        sm.alpha_ref = mat.alpha_ref;
        sm.stages = mat.stages;
        sm.address_u = mat.address_u;
        sm.address_v = mat.address_v;
        sm.stage_uv = stage_uv;
        sm.direct_uv = direct_uv;
        sm.pixel_shader = active_pixel_shader;
        sm.has_pixel_shader = has_pixel_shader;
        sm.render_mode = render_mode;
        sm.colour_source = colour_source;
        sm.skin_weight_mask = skin_weight_mask;
        sm.uses_skin_palette = uses_skin_palette;
        sm.node_state_slot = static_cast<std::int8_t>(cpu[node + 0x3D]);
        sm.node_state_value = cpu[node + 0x3E];
        if (has_pixel_shader)
        {
            // CreatePixelShader's mappings select the constants subsequently
            // written by Xbox 0x0015AD20, independently of sampler names.
            for (u32 i = 0; i < 8; ++i)
            {
                const u32 c0 = (sm.pixel_shader[57] >> (i * 4)) & 15;
                const u32 c1 = (sm.pixel_shader[58] >> (i * 4)) & 15;
                if (c0 < mat.constants.size())
                {
                    sm.pixel_shader[10 + i] = mat.constants[c0];
                }
                if (c1 < mat.constants.size())
                {
                    sm.pixel_shader[18 + i] = mat.constants[c1];
                }
            }
        }
        mesh.submeshes.push_back(sm);
    };

    // Xbox 0x00158160 relocates the index-pointer array, primitive array,
    // and count array. 0x001582A0 draws every section, not only count == 1.
    auto emit_push = [&](size_t node, size_t tag, u32 sections, const ActiveMaterial& mat,
                         const std::vector<u8>& skin, const std::vector<u16>& mtx)
    {
        const size_t counts = tag + 16;
        const size_t pointers = counts + size_t(sections) * 4;
        const size_t primitives = pointers + size_t(sections) * 4;
        if (primitives > cpu.size() || size_t(sections) * 4 > cpu.size() - primitives)
        {
            throw Error("ndPushBuffer section arrays out of range");
        }
        for (u32 i = 0; i < sections; ++i)
        {
            const size_t index_offset = size_t(kNodeBase) + read_u32(cpu, pointers + size_t(i) * 4);
            emit_indexed(node, index_offset, read_u32(cpu, counts + size_t(i) * 4),
                         read_u32(cpu, primitives + size_t(i) * 4), mat, skin, mtx);
        }
    };

    ActiveMaterial active;
    std::vector<u8> active_skin;
    std::unordered_map<u32, u32> parent_of;
    {
        struct Link
        {
            u32 rel;
            u32 parent;
        };
        std::vector<Link> links{{52, 0}};
        std::vector<u8> link_seen(cpu.size() / 4 + 1, 0);
        while (!links.empty())
        {
            const Link link = links.back();
            links.pop_back();
            if (link.rel == 0 || size_t(link.rel) / 4 >= link_seen.size() || link_seen[link.rel / 4])
            {
                continue;
            }
            link_seen[link.rel / 4] = 1;
            parent_of[link.rel] = link.parent;
            const HeaderItem hi = read_item(link.rel);
            if (hi.next_sibling)
            {
                links.push_back({hi.next_sibling, link.parent});
            }
            if (hi.next_child)
            {
                links.push_back({hi.next_child, link.rel});
            }
        }
    }
    auto mtx_at = [&](u32 rel) -> std::vector<u16>
    {
        for (u32 n = rel; n != 0;)
        {
            const HeaderItem hi = read_item(n);
            if (hi.type == 12)
            {
                std::vector<u16> joints;
                const size_t p = size_t(kNodeBase) + hi.un_other6;
                if (hi.count > 0 && hi.count <= 256 && p + size_t(hi.count) * 8 <= cpu.size())
                {
                    joints.resize(hi.count);
                    for (u32 i = 0; i < hi.count; ++i)
                    {
                        const size_t rec = p + size_t(i) * 8;
                        const u8 flag = cpu[rec + 5];
                        joints[i] = (flag & 2) ? read_u16(cpu, rec) : u16(cpu[rec + 6]);
                    }
                }
                return joints;
            }
            const auto it = parent_of.find(n);
            if (it == parent_of.end())
            {
                break;
            }
            n = it->second;
        }
        return {};
    };
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
        if (hi.type == 19 || hi.type == 9 || hi.type == 23)
        {
            if ((hi.type == 9 || hi.type == 23) && read_u16(cpu, size_t(kNodeBase) + rel + 36) > 1)
            {
                mesh.issues.push_back("blendshape primary buffer loaded; animated targets require translation");
            }
            for (size_t i = 0; i < streams.size(); ++i)
            {
                if (streams[i].cpu_off == size_t(kNodeBase) + rel)
                {
                    active_stream = i;
                    break;
                }
            }
        }
        else if (hi.type == 17)
        {
            const size_t wrapper = size_t(kNodeBase) + hi.un_other6;
            has_pixel_shader = false;
            if (wrapper + 12 <= cpu.size())
            {
                // Xbox 0x00157CE0 / 0x0015B5E0 selects render states from wrapper[0].
                render_mode = i32(read_u32(cpu, wrapper));
                const size_t definition = size_t(kNodeBase) + read_u32(cpu, wrapper + 4);
                if (read_u32(cpu, wrapper + 8) == 240 && definition + 240 <= cpu.size())
                {
                    for (size_t i = 0; i < active_pixel_shader.size(); ++i)
                    {
                        active_pixel_shader[i] = read_u32(cpu, definition + i * 4);
                    }
                    has_pixel_shader = true;
                }
            }
        }
        else if (hi.type == 21)
        {
            // Xbox 0x00158510 binds each stream through the descriptor indices
            // at +32. Xbox 0x001584A0 reads declaration +80 and program +84.
            const size_t node = size_t(kNodeBase) + rel;
            direct_uv = false;
            stage_uv.fill(255);
            colour_source.fill(255);
            skin_weight_mask = 0;
            uses_skin_palette = false;
            if (node + 104 <= cpu.size())
            {
                const size_t si = active_stream;
                const auto& attributes = streams[si].attribute_types;
                const size_t bindings = size_t(kNodeBase) + hi.un_other6;
                std::array<u8, 16> registers{};
                registers.fill(255);
                u32 stream = 0;
                size_t decl = size_t(kNodeBase) + read_u32(cpu, node + 80);
                while (decl + 4 <= cpu.size())
                {
                    const u32 token = read_u32(cpu, decl);
                    decl += 4;
                    if (token == 0xffffffff)
                    {
                        break;
                    }
                    const u32 kind = token >> 29;
                    if (kind == 1)
                    {
                        stream = token & 15;
                    }
                    else if (kind == 2 && (token & 0x10000000) == 0 && stream < hi.count &&
                             bindings + size_t(stream) * 4 + 4 <= cpu.size())
                    {
                        const u32 descriptor = read_u32(cpu, bindings + size_t(stream) * 4);
                        if (descriptor < attributes.size() && (token & 31) < registers.size())
                        {
                            registers[token & 31] = attributes[descriptor];
                        }
                    }
                }
                const size_t program = size_t(kNodeBase) + read_u32(cpu, node + 84);
                if (program + 4 <= cpu.size())
                {
                    const u32 header = read_u32(cpu, program);
                    const u32 instructions = header >> 16;
                    if ((header & 0xffff) == 0x2078 && program + 4 + size_t(instructions) * 16 <= cpu.size())
                    {
                        for (u32 i = 0; i < instructions; ++i)
                        {
                            const size_t q = program + 4 + size_t(i) * 16;
                            const u32 w1 = read_u32(cpu, q + 4);
                            const u32 w2 = read_u32(cpu, q + 8);
                            const u32 w3 = read_u32(cpu, q + 12);
                            const u32 mac = (w1 >> 21) & 15;
                            // Operand arity: ADD consumes A,C; ILU consumes C.
                            constexpr u8 operands[14] = {0, 1, 3, 5, 7, 3, 3, 3, 3, 3, 3, 3, 3, 1};
                            const u8 used = (mac < 14 ? operands[mac] : 0) | (((w1 >> 25) & 7) ? 4 : 0);
                            const u8 input_attr = registers[(w1 >> 9) & 15];
                            if (input_attr == 0 || input_attr == 8)
                            {
                                const u32 mux[3] = {(w2 >> 26) & 3, (w2 >> 11) & 3, (w3 >> 28) & 3};
                                const u32 swizzle[3] = {w1 & 255, (w2 >> 17) & 255, (w2 >> 2) & 255};
                                for (u32 operand = 0; operand < 3; ++operand)
                                {
                                    if ((used & (1 << operand)) && mux[operand] == 2)
                                    {
                                        if (input_attr == 0)
                                        {
                                            uses_skin_palette = true;
                                            continue;
                                        }
                                        for (u32 component = 0; component < 4; ++component)
                                        {
                                            skin_weight_mask |= u8(1 << ((swizzle[operand] >> (component * 2)) & 3));
                                        }
                                    }
                                }
                            }
                            const u32 output = (w3 >> 3) & 255;
                            // The shared output port selects MAC A or ILU C with
                            // OUT_MUX. Retail background programs pair DP3 with
                            // an ILU MOV of a UV input in the same instruction.
                            const bool ilu_output = (w3 & 4) != 0;
                            const bool direct_input = ilu_output
                                ? (((w1 >> 25) & 7) == 1 && ((w3 >> 28) & 3) == 2 && (w2 & 0x400) == 0)
                                : (mac == 1 && ((w2 >> 26) & 3) == 2 && (w1 & 0x100) == 0);
                            const u32 output_swizzle = ilu_output ? ((w2 >> 2) & 255) : (w1 & 255);
                            if ((w3 & 0x800) != 0 && (output == 3 || output == 4) && (w3 & 0xf000) != 0)
                            {
                                colour_source[output - 3] = 255;
                                const u8 attr = registers[(w1 >> 9) & 15];
                                if ((w3 & 0xf000) == 0xf000 && direct_input && output_swizzle == 0x1b &&
                                    (attr == 11 || attr == 12))
                                {
                                    colour_source[output - 3] = attr - 11;
                                }
                            }
                            if ((w3 & 0x800) != 0 && output >= 9 && output <= 12 && ((w3 >> 12) & 15) != 0)
                            {
                                const u8 attr = registers[(w1 >> 9) & 15];
                                // Full xy MOV from an input register is a direct UV binding.
                                if (direct_input && (output_swizzle & 0xf0) == 0x10 &&
                                    (w3 & 0xc000) == 0xc000 && attr >= 13 && attr <= 16)
                                {
                                    stage_uv[output - 9] = attr - 13;
                                }
                                else
                                {
                                    stage_uv[output - 9] = 255;
                                }
                            }
                        }
                        direct_uv = has_pixel_shader;
                        for (u32 i = 0; i < 4; ++i)
                        {
                            const u32 mode = (active_pixel_shader[54] >> (i * 5)) & 31;
                            if (mode != 0 && (mode != 1 || stage_uv[i] == 255))
                            {
                                direct_uv = false;
                            }
                        }
                    }
                }
            }
        }
        else if (hi.type == kNdShaderParam2)
        {
            const auto named = node_names.find(rel);
            active_shader_name = named == node_names.end() ? std::string{} : named->second;
            active = parse_param(hi.un_other6);
        }
        else if (hi.type == 12)
        {
            // ndMtxArray. 0x155E60 walks 8-byte records. Flag +5 bit 1 selects the
            // skeleton joint in the u16 at +0; bit 0 selects the byte at +6.
            // 0x157B90 then indexes this table with the ndRigidSkinIdx byte.
            mesh.mtx_joints.clear();
            const size_t p = size_t(kNodeBase) + hi.un_other6;
            if (hi.count > 0 && hi.count <= 256 && p + size_t(hi.count) * 8 <= cpu.size())
            {
                mesh.mtx_joints.resize(hi.count);
                for (u32 i = 0; i < hi.count; ++i)
                {
                    const size_t rec = p + size_t(i) * 8;
                    const u8 flag = cpu[rec + 5];
                    mesh.mtx_joints[i] = (flag & 2) ? read_u16(cpu, rec) : u16(cpu[rec + 6]);
                }
            }
        }
        else if (hi.type == 11)
        {
            // 0x157B90 uploads palette[i] at constant 0x78 + i*3. The byte count is
            // header +36. The gap to the next window over-reads the last palette.
            active_skin.clear();
            const size_t len = hi.count;
            const size_t p = size_t(kNodeBase) + hi.un_other6;
            if (p > cpu.size() || len > cpu.size() - p)
            {
                throw Error("skin palette out of range");
            }
            active_skin.assign(cpu.begin() + p, cpu.begin() + p + len);
        }
        else if (hi.type == kNdPushBuffer || hi.type == kNdBGPushBuffer)
        {
            // header +32 is the section count. +36 selects a hardware push
            // resource; zero selects the three arrays consumed by 0x1582A0.
            if (hi.count == 0)
            {
                const size_t tag = size_t(kNodeBase) + hi.offset;
                emit_push(size_t(kNodeBase) + rel, tag, hi.un_other6, active, active_skin, mtx_at(rel));
            }
            else
            {
                mesh.issues.push_back("hardware push buffer requires translation");
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
            catch (const std::exception& ex)
            {
                mesh.issues.push_back("texture " + std::to_string(t) + ": " + ex.what());
            }
            // Byte +0xE of the texture. 0x000527C0 registers it when the byte is in 1..0x27.
            // decode_xbox_texture replaces the image, so the slot is stored after that.
            const u8 slot = u8((hdr.flags >> 16) & 0xff);
            if (slot >= 1 && slot <= 0x27)
            {
                mesh.textures[t].movie_slot = slot;
            }
        }
    }
}

} // namespace

// Xbox 0x00158240. The render context's +0x1F8 points at instance +0x294.
bool model_submesh_visible(const Mesh& mesh, std::int8_t slot, u8 value)
{
    if (slot == -1)
    {
        return true;
    }
    const int bit = int(slot) * 2;
    const int byte = bit / 8;
    const int shift = bit % 8;
    return ((mesh.node_flags.at(size_t(byte)) >> (u8(shift) & 31)) & 3) == value;
}

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
        mesh.issues.insert(mesh.issues.end(), s.issues.begin(), s.issues.end());
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
        mesh.diffuse.resize(mesh.positions.size(), 0xffffffff);
        mesh.specular.resize(mesh.positions.size(), 0);
        for (size_t i = 0; i < s.diffuse.size(); ++i)
        {
            mesh.diffuse[bases.back() + i] = s.diffuse[i];
        }
        for (size_t i = 0; i < s.specular.size(); ++i)
        {
            mesh.specular[bases.back() + i] = s.specular[i];
        }
        for (size_t i = 0; i < s.normals.size(); ++i)
        {
            mesh.normals[bases.back() + i] = s.normals[i];
        }
        mesh.uvs.resize(mesh.positions.size());
        mesh.uv1.resize(mesh.positions.size());
        mesh.uv2.resize(mesh.positions.size());
        mesh.uv3.resize(mesh.positions.size());
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
        for (size_t i = 0; i < s.uv3.size(); ++i)
        {
            mesh.uv3[bases.back() + i] = s.uv3[i];
        }
        if (any_bones)
        {
            mesh.bones.resize(mesh.positions.size(), 0);
            mesh.bone2.resize(mesh.positions.size(), 0);
            mesh.bone3.resize(mesh.positions.size(), 0);
            mesh.bone4.resize(mesh.positions.size(), 0);
            mesh.weight0.resize(mesh.positions.size(), 1.f);
            mesh.weight1.resize(mesh.positions.size(), 0.f);
            mesh.weight2.resize(mesh.positions.size(), 0.f);
            mesh.weight3.resize(mesh.positions.size(), 0.f);
            for (size_t i = 0; i < s.bones.size(); ++i)
            {
                mesh.bones[bases.back() + i] = s.bones[i];
            }
            for (size_t i = 0; i < s.bone2.size(); ++i)
            {
                mesh.bone2[bases.back() + i] = s.bone2[i];
                mesh.bone3[bases.back() + i] = s.bone3[i];
                mesh.bone4[bases.back() + i] = s.bone4[i];
            }
            for (size_t i = 0; i < s.weight0.size(); ++i)
            {
                mesh.weight0[bases.back() + i] = s.weight0[i];
                mesh.weight1[bases.back() + i] = s.weight1[i];
                mesh.weight2[bases.back() + i] = s.weight2[i];
                mesh.weight3[bases.back() + i] = s.weight3[i];
            }
        }
    }
    parse_skeleton(entry.cpu, mesh);
    parse_scene_graph(entry.cpu, mesh, streams, bases);
    std::vector<i32> tex_chunks;
    parse_embedded_textures(entry.cpu, entry.gpu, entry.gpu_chunks, mesh, tex_chunks);
    (void)tex_chunks;
    for (auto& sm : mesh.submeshes)
    {
        auto valid_surface = [&](i32 i) -> bool
        {
            return i >= 0 && size_t(i) < mesh.textures.size() && !mesh.textures[i].bgra.empty();
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

        if (sm.lightmap >= 0 && size_t(sm.lightmap) < mesh.textures.size())
        {
            if (mesh.textures[sm.lightmap].bgra.empty())
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
