#if defined(GBTG_INSPECTOR_APP) && !defined(GBTG_RETAIL_GAME_ONLY)
#include "serialized_model.h"
#include "gbtg/model_nodes.h"

#include <algorithm>
#include <cmath>

namespace gbtg
{
std::span<const u8> GpuSpace::at(u32 offset, size_t size) const
{
    u32 start = 0;
    for (const auto& chunk : entry->gpu_chunks)
    {
        if (offset >= start && offset - start < chunk.size)
        {
            const size_t inside = offset - start;
            if (size > chunk.size - inside || chunk.offset + chunk.size > entry->gpu.size())
            {
                throw Error("serialized model GPU data crosses its chunk");
            }
            return entry->gpu.subspan(chunk.offset + inside, size);
        }
        start += chunk.size;
    }
    throw Error("serialized model GPU offset outside its chunks");
}

namespace
{
// A vertex buffer record (0x18 bytes): its kind byte (+1), stride byte (+0), GPU data (+0x10) and size (+0x14).
struct BufferRecord
{
    u8 kind = 0;
    u8 stride = 0;
    u32 data = 0;
    u32 size = 0;
};

std::vector<BufferRecord> read_buffers(const SerializedImage& image, u32 tree, u32 table)
{
    std::vector<BufferRecord> buffers;
    const u32 count = image.get(table, vertex_buffer_node::buffer_count);
    const u32 records = tree + image.get(table, vertex_buffer_node::buffers);
    for (u32 index = 0; index < count; ++index)
    {
        const u32 record = records + index * model_vertex_buffer_field::kSize;
        buffers.push_back({image.byte(record, 1), image.byte(record), image.word(record, 0x10), image.word(record, 0x14)});
    }
    return buffers;
}

// What a node's subtree inherits from the nodes above it.
struct WalkState
{
    std::vector<BufferRecord> buffers; // The current vertex buffer node's (or blend-shape base's).
    std::vector<std::vector<BufferRecord>> targets; // The blend shape's other targets.
    std::vector<u16> matrix_joints; // ndMtxArray: row -> skeleton joint.
    std::vector<u32> streams; // The vertex shader's buffers, by declaration stream.
    u32 material = 0;
    std::vector<u8> palette; // ndRigidSkinIdx: shader slot -> matrix row.
    bool head = false;
};

struct Walker
{
    const SerializedImage& image;
    const GpuSpace& gpu;
    u32 tree;
    DecodedModel& model;
    std::map<u32, std::string> shader_names;
    std::map<std::pair<bool, u32>, size_t> primitive_of;
    // Per primitive: (source vertex, joint pair) -> output vertex.
    std::vector<std::map<std::tuple<u32, u16, u16>, u32>> remap;
    std::vector<u32> body_triangles_facing;

    float read_float(const BufferRecord& buffer, u32 vertex, u32 component) const
    {
        const auto bytes = gpu.at(buffer.data + vertex * buffer.stride + component * 4, 4);
        float value;
        std::memcpy(&value, bytes.data(), 4);
        return value;
    }

    const BufferRecord* stream_of_kind(const WalkState& state, u8 kind) const
    {
        for (const u32 stream : state.streams)
        {
            if (stream < state.buffers.size() && state.buffers[stream].kind == kind && state.buffers[stream].size != 0)
            {
                return &state.buffers[stream];
            }
        }
        return nullptr;
    }

    u16 slot_joint(const WalkState& state, float value) const
    {
        // The bone stream holds the slot's constant register (24 + 3 * slot).
        const int slot = int(std::lround((value - 24.f) / 3.f));
        if (slot < 0 || size_t(slot) >= state.palette.size())
        {
            throw Error("vertex bone slot " + std::to_string(slot) + " is outside its skin palette");
        }
        const u8 row = state.palette[size_t(slot)];
        if (row >= state.matrix_joints.size())
        {
            throw Error("skin palette row outside the matrix array");
        }
        return state.matrix_joints[row];
    }

    void draw(const WalkState& state, u32 indices, u32 primitive, u32 count)
    {
        const BufferRecord* positions = stream_of_kind(state, 9);
        if (positions == nullptr)
        {
            throw Error("a draw has no position stream");
        }
        const BufferRecord* normals = stream_of_kind(state, 0xA);
        const BufferRecord* uvs = stream_of_kind(state, 0xD);
        const BufferRecord* bones = stream_of_kind(state, 0);
        const BufferRecord* weights = stream_of_kind(state, 8);
        const BufferRecord* colours = stream_of_kind(state, 0xB);
        const auto key = std::pair{state.head, state.material};
        auto found = primitive_of.find(key);
        if (found == primitive_of.end())
        {
            ModelPrimitive primitive_record;
            const auto name = shader_names.find(state.material);
            primitive_record.material = name != shader_names.end() ? name->second
                : "material_" + std::to_string(state.material);
            primitive_record.material_node = state.material;
            primitive_record.head = state.head;
            primitive_record.morphs.resize(state.head ? state.targets.size() : 0);
            found = primitive_of.emplace(key, model.primitives.size()).first;
            model.primitives.push_back(std::move(primitive_record));
            remap.emplace_back();
        }
        ModelPrimitive& out = model.primitives[found->second];
        auto& lookup = remap[found->second];
        const auto vertex_of = [&](u32 source) -> u32
        {
            if (positions->stride == 0 || u64(source + 1) * positions->stride > positions->size)
            {
                throw Error("a draw index is outside its vertex buffer");
            }
            ModelVertex vertex;
            vertex.position = {read_float(*positions, source, 0), read_float(*positions, source, 1),
                read_float(*positions, source, 2)};
            if (normals != nullptr)
            {
                vertex.normal = {read_float(*normals, source, 0), read_float(*normals, source, 1),
                    read_float(*normals, source, 2)};
            }
            if (uvs != nullptr)
            {
                vertex.uv = {read_float(*uvs, source, 0), read_float(*uvs, source, 1)};
            }
            if (colours != nullptr)
            {
                const auto bytes = gpu.at(colours->data + source * colours->stride, 4);
                std::memcpy(&vertex.colour, bytes.data(), 4);
            }
            if (bones != nullptr)
            {
                vertex.joints[0] = slot_joint(state, read_float(*bones, source, 0));
                vertex.joints[1] = vertex.joints[0];
                if (bones->stride >= 8)
                {
                    const float second = read_float(*bones, source, 1);
                    if (second != 0.f)
                    {
                        vertex.joints[1] = slot_joint(state, second);
                    }
                }
            }
            if (weights != nullptr && bones != nullptr && bones->stride >= 8)
            {
                vertex.weights = {read_float(*weights, source, 0),
                    weights->stride >= 8 ? read_float(*weights, source, 1) : 0.f};
            }
            if (vertex.joints[1] == vertex.joints[0] || !(vertex.weights[0] + vertex.weights[1] > 0.f))
            {
                vertex.weights = {1.f, 0.f};
            }
            const auto key = std::tuple{source, vertex.joints[0], vertex.joints[1]};
            if (const auto known = lookup.find(key); known != lookup.end())
            {
                return known->second;
            }
            const u32 index = u32(out.vertices.size());
            out.vertices.push_back(vertex);
            for (size_t target = 0; target < out.morphs.size(); ++target)
            {
                Vec3 delta{};
                for (const BufferRecord& buffer : state.targets[target])
                {
                    // A target holds offsets from the base: 0x00157650 adds weight * target to it.
                    if (buffer.kind == 9 && buffer.size >= u64(source + 1) * buffer.stride)
                    {
                        delta = {read_float(buffer, source, 0), read_float(buffer, source, 1),
                            read_float(buffer, source, 2)};
                        break;
                    }
                }
                out.morphs[target].push_back(delta);
            }
            lookup.emplace(key, index);
            return index;
        };
        std::vector<u16> list(count);
        for (u32 index = 0; index < count; ++index)
        {
            list[index] = image.half(tree + indices, index * 2);
        }
        const auto triangle = [&](u16 a, u16 b, u16 c)
        {
            if (a == b || b == c || a == c)
            {
                return;
            }
            out.indices.push_back(vertex_of(a));
            out.indices.push_back(vertex_of(b));
            out.indices.push_back(vertex_of(c));
        };
        if (primitive == 5)
        {
            for (u32 index = 0; index + 2 < count; index += 3)
            {
                triangle(list[index], list[index + 1], list[index + 2]);
            }
        }
        else if (primitive == 6)
        {
            for (u32 index = 0; index + 2 < count; ++index)
            {
                if ((index & 1) == 0)
                {
                    triangle(list[index], list[index + 1], list[index + 2]);
                }
                else
                {
                    triangle(list[index + 1], list[index], list[index + 2]);
                }
            }
        }
        else
        {
            throw Error("draw primitive " + std::to_string(primitive) + " is not a triangle list or strip");
        }
    }

    void chain(u32 node, const WalkState& parent)
    {
        for (; node != 0; node = image.get(tree + node, model_node_field::next))
        {
            const u32 at = tree + node;
            const std::string type = image.text(tree + image.word(at));
            WalkState state = parent;
            if (type == "ndVertexBuffer")
            {
                state.buffers = read_buffers(image, tree, at);
            }
            else if (type == "ndBlendShape")
            {
                const u32 count = image.get(at, blendshape_node::target_count);
                const u32 targets = tree + image.get(at, blendshape_node::targets);
                state.head = true;
                state.targets.clear();
                for (u32 target = 0; target < count; ++target)
                {
                    const auto buffers = read_buffers(image, tree, tree + image.word(targets, target * 4));
                    if (target == 0)
                    {
                        state.buffers = buffers;
                    }
                    else
                    {
                        state.targets.push_back(buffers);
                    }
                }
            }
            else if (type == "ndMtxArray")
            {
                state.matrix_joints.clear();
                const u32 rows = tree + image.get(at, matrix_array_node::rows);
                for (i32 row = 0; row < image.get(at, matrix_array_node::row_count); ++row)
                {
                    // Flag +5 bit 1 selects the joint in the u16 at +0, else the byte at +6 (0x00155E60).
                    const u32 record = rows + u32(row) * 8;
                    state.matrix_joints.push_back((image.byte(record, 5) & 2) != 0 ? image.half(record)
                        : u16(image.byte(record, 6)));
                }
            }
            else if (type == "ndVertexShader" || type == "ndVtxDesc")
            {
                state.streams.clear();
                const u32 list = tree + image.get(at, vertex_shader_node::streams);
                for (u32 stream = 0; stream < image.get(at, vertex_shader_node::stream_count); ++stream)
                {
                    state.streams.push_back(image.word(list, stream * 4));
                }
            }
            else if (type == "ndShaderParam2")
            {
                state.material = node;
            }
            else if (type == "ndRigidSkinIdx")
            {
                state.palette.clear();
                const u32 list = tree + image.get(at, rigid_skin_node::indices);
                for (u32 index = 0; index < image.get(at, rigid_skin_node::index_count); ++index)
                {
                    state.palette.push_back(image.byte(list, index));
                }
            }
            else if (type == "ndPushBuffer" || type == "ndBGPushBuffer")
            {
                if (image.get(at, push_buffer_node::header) != 0)
                {
                    throw Error("a recorded push buffer is not supported by the exporter");
                }
                for (u32 index = 0; index < image.get(at, push_buffer_node::draw_count); ++index)
                {
                    draw(state, image.word(tree + image.get(at, push_buffer_node::draw_indices), index * 4),
                        image.word(tree + image.get(at, push_buffer_node::draw_primitives), index * 4),
                        image.word(tree + image.get(at, push_buffer_node::draw_counts), index * 4));
                }
            }
            const u32 child = image.get(at, model_node_field::child);
            if (child != 0)
            {
                chain(child, state);
            }
        }
    }
};

void read_material(const SerializedImage& image, u32 tree, u32 node, ModelMaterialInfo& info)
{
    const u32 material = tree + image.get(tree + node, shader_node::primary);
    const u32 stages = tree + image.get(material, model_material_field::stages);
    const u32 stage_total = image.get(material, model_material_field::stage_count);
    const u32 entries = image.get(material, model_material_field::entries);
    for (u32 entry = 0; entries != 0 && entry < image.get(material, model_material_field::entry_count); ++entry)
    {
        const u32 record = tree + entries + entry * model_material_entry_field::kSize;
        const std::string name = image.text(tree + image.get(record, model_material_entry_field::name));
        const i32 stage = image.get(record, model_material_entry_field::value);
        if (stage < 0 || u32(stage) >= stage_total)
        {
            continue;
        }
        const i32 texture = i32(image.get(stages + u32(stage) * model_material_stage_field::kSize,
            model_material_stage_field::texture));
        if (name == "colour0")
        {
            info.colour_texture = texture;
        }
        else if (name == "RARE_celmap0")
        {
            info.celmap_texture = texture;
        }
        else if (name == "RARE_lightmap0")
        {
            info.lightmap_texture = texture;
        }
    }
}
}

DecodedModel decode_serialized_model(const BundleEntry& entry)
{
    const SerializedImage image{entry.cpu};
    const GpuSpace gpu{&entry};
    DecodedModel model;
    model.aid = entry.name;
    const u32 entries = image.word(0);
    u32 tree = 0;
    u32 textures = 0;
    for (u32 index = 0; index < image.word(0, 4); ++index)
    {
        const u32 kind = image.word(entries, index * 8);
        const u32 block = image.word(entries, index * 8 + 4);
        if (kind == 0)
        {
            tree = image.get(block, model_tree_block_field::tree);
        }
        else if (kind == 7)
        {
            textures = block;
        }
    }
    if (tree == 0)
    {
        throw Error(entry.name + " has no node tree");
    }
    Walker walker{image, gpu, tree, model};
    std::map<u32, std::string> joint_names;
    const u32 names = image.get(tree, model_tree_field::names);
    std::vector<std::pair<std::string, u32>> morph_slots;
    if (names != 0)
    {
        for (u32 name = 0; name < image.word(tree + names); ++name)
        {
            const u32 record = tree + image.word(tree + names, 4) + name * 0xC;
            const std::string text = image.text(tree + image.word(record));
            const u32 value = image.word(tree + image.word(record, 4));
            if (text.starts_with("shader_"))
            {
                walker.shader_names.emplace(value, text);
            }
            else if (text.starts_with("MORPH_TARGET_"))
            {
                morph_slots.emplace_back(text, value);
            }
            else if (!text.starts_with("|") && !text.starts_with("texturefile") && text != "numTextures")
            {
                joint_names.emplace(value, text);
            }
        }
    }
    // The skeleton (the first ndSkeleton reached from the roots).
    const u32 roots = tree + image.get(tree, model_tree_field::roots);
    for (u32 root = 0; root < image.get(tree, model_tree_field::root_count) && model.joints.empty(); ++root)
    {
        for (u32 node = image.word(roots, root * 4); node != 0; node = image.get(tree + node, model_node_field::next))
        {
            if (image.text(tree + image.word(tree + node)) == "ndSkeleton")
            {
                const u32 joints = tree + image.get(tree + node, skeleton_node::joints);
                for (u32 joint = 0; joint < image.get(tree + node, skeleton_node::joint_count); ++joint)
                {
                    const u32 record = joints + joint * skeleton_joint_field::kSize;
                    ModelJoint out;
                    const u16 parent = image.get(record, skeleton_joint_field::parent);
                    out.parent = parent == 0xFFFF ? -1 : int(parent);
                    out.world = image.get(record, skeleton_joint_field::world);
                    const auto name = joint_names.find(joint);
                    out.name = name != joint_names.end() ? name->second : "joint_" + std::to_string(joint);
                    model.joints.push_back(out);
                }
                break;
            }
        }
    }
    for (u32 root = 0; root < image.get(tree, model_tree_field::root_count); ++root)
    {
        walker.chain(image.word(roots, root * 4), WalkState{});
    }
    // MORPH names by the auxiliary slot they drive, in slot order; one per target after the base.
    std::sort(morph_slots.begin(), morph_slots.end(), [](const auto& a, const auto& b)
    {
        return a.second < b.second;
    });
    size_t targets = 0;
    for (const auto& primitive : model.primitives)
    {
        targets = std::max(targets, primitive.morphs.size());
    }
    for (size_t target = 0; target < targets; ++target)
    {
        model.morph_names.push_back(target < morph_slots.size() ? morph_slots[target].first
            : "target_" + std::to_string(target + 1));
    }
    // Strips have one winding; emit triangles so most faces agree with their vertex normals.
    double agree = 0.0;
    for (const auto& primitive : model.primitives)
    {
        for (size_t index = 0; index + 2 < primitive.indices.size(); index += 3)
        {
            const Vec3& a = primitive.vertices[primitive.indices[index]].position;
            const Vec3& b = primitive.vertices[primitive.indices[index + 1]].position;
            const Vec3& c = primitive.vertices[primitive.indices[index + 2]].position;
            const Vec3 u{b.x - a.x, b.y - a.y, b.z - a.z};
            const Vec3 v{c.x - a.x, c.y - a.y, c.z - a.z};
            const Vec3 face{u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x};
            Vec3 normal{};
            for (int corner = 0; corner < 3; ++corner)
            {
                const Vec3& n = primitive.vertices[primitive.indices[index + size_t(corner)]].normal;
                normal = {normal.x + n.x, normal.y + n.y, normal.z + n.z};
            }
            agree += (face.x * normal.x + face.y * normal.y + face.z * normal.z) > 0.f ? 1.0 : -1.0;
        }
    }
    if (agree < 0.0)
    {
        model.winding_flipped = true;
        for (auto& primitive : model.primitives)
        {
            for (size_t index = 0; index + 2 < primitive.indices.size(); index += 3)
            {
                std::swap(primitive.indices[index + 1], primitive.indices[index + 2]);
            }
        }
    }
    // Materials and their textures.
    for (const auto& primitive : model.primitives)
    {
        if (std::any_of(model.materials.begin(), model.materials.end(), [&](const ModelMaterialInfo& known)
            {
                return known.node == primitive.material_node;
            }))
        {
            continue;
        }
        ModelMaterialInfo info;
        info.name = primitive.material;
        info.node = primitive.material_node;
        read_material(image, tree, primitive.material_node, info);
        model.materials.push_back(info);
    }
    if (textures != 0)
    {
        for (u32 index = 0; index < image.get(textures, model_texture_table_field::count); ++index)
        {
            const u32 record = image.word(image.get(textures, model_texture_table_field::textures), index * 4);
            ModelTextureInfo info;
            info.index = index;
            info.format = image.get(record, model_texture_field::format);
            info.width = image.get(record, model_texture_field::width);
            info.height = image.get(record, model_texture_field::height);
            info.data = image.get(record, model_texture_field::data);
            info.frame_size = image.get(record, model_texture_field::frame_size);
            if (info.frame_size != 0)
            {
                TextureHeader header;
                header.format = XboxFormat(info.format);
                header.width = info.width;
                header.height = info.height;
                header.data_size = info.frame_size;
                header.texture_offset = 0;
                header.tile_count = 1;
                info.image = decode_xbox_texture(header, gpu.at(info.data, info.frame_size));
            }
            model.textures.push_back(std::move(info));
        }
    }
    return model;
}
}
#endif
