#if defined(GBTG_INSPECTOR_APP) && !defined(GBTG_RETAIL_GAME_ONLY)
#include "model_import.h"
#include "serialized_model.h"
#include "gbtg/model_nodes.h"
#include "gbtg/resource.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <map>
#include <set>

namespace gbtg
{
namespace
{
constexpr u32 kGpuAlign = 0x80;

struct Group
{
    u32 rigid = 0; // Absolute CPU offsets.
    u32 push = 0;
    u32 rigid_size = 0;
};

// One ndShaderParam2 under one vertex shader: a material variant, and the palette groups that draw with it.
struct Variant
{
    bool head = false;
    std::string shader;
    u32 node = 0; // ndShaderParam2, tree-relative.
    std::vector<Group> groups;
    bool blended = false; // Its template vertices use a second bone.
    int base = -1; // A new material: the template variant it is a copy of (its node is written with the groups).
    i32 texture = -1; // A new material: its new colour texture's index, or -1 to keep the base's.
};

struct Branch
{
    u32 table = 0; // Absolute: the node whose +0x20 / +0x24 are its vertex buffer records.
    std::vector<u32> targets; // Absolute: the blend shape's other target tables.
    u32 matrix_node = 0; // Absolute ndMtxArray.
    std::vector<std::array<u8, 8>> rows;
    u32 palette_cap = 0;
    bool present = false;
};

struct Template
{
    SerializedImage image;
    u32 tree = 0;
    u32 textures = 0;
    std::vector<Variant> variants;
    Branch branches[2]; // Body, head.
};

std::string lower(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c)
    {
        return char(std::tolower(c));
    });
    return text;
}

// A turn (an axis permutation with signs), uniform scale and offset taking the file's space to the template's.
struct SkeletonFit
{
    std::array<int, 3> axis{0, 1, 2};
    std::array<float, 3> sign{1.f, 1.f, 1.f};
    float scale = 1.f;
    Vec3 offset{};
    bool identity = true;
    bool turned = false;
    bool mirrored = false;

    Vec3 turn(const Vec3& v) const
    {
        const float in[3]{v.x, v.y, v.z};
        return {sign[0] * in[axis[0]], sign[1] * in[axis[1]], sign[2] * in[axis[2]]};
    }
    Vec3 scaled(const Vec3& v) const
    {
        const Vec3 t = turn(v);
        return {t.x * scale, t.y * scale, t.z * scale};
    }
    Vec3 point(const Vec3& v) const
    {
        const Vec3 t = scaled(v);
        return {t.x + offset.x, t.y + offset.y, t.z + offset.z};
    }
};

SkeletonFit fit_skeleton(const std::vector<std::pair<Vec3, Vec3>>& pairs)
{
    SkeletonFit best;
    if (pairs.size() < 3)
    {
        return best;
    }
    const auto length = [](const Vec3& v)
    {
        return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    };
    float best_error = 1e30f;
    std::array<int, 3> axis{0, 1, 2};
    do
    {
        for (int signs = 0; signs < 8; ++signs)
        {
            SkeletonFit candidate;
            candidate.axis = axis;
            candidate.sign = {signs & 1 ? -1.f : 1.f, signs & 2 ? -1.f : 1.f, signs & 4 ? -1.f : 1.f};
            Vec3 from{};
            Vec3 to{};
            for (const auto& [a, b] : pairs)
            {
                const Vec3 t = candidate.turn(a);
                from = {from.x + t.x, from.y + t.y, from.z + t.z};
                to = {to.x + b.x, to.y + b.y, to.z + b.z};
            }
            const float count = float(pairs.size());
            from = {from.x / count, from.y / count, from.z / count};
            to = {to.x / count, to.y / count, to.z / count};
            float spread_from = 0.f;
            float spread_to = 0.f;
            for (const auto& [a, b] : pairs)
            {
                const Vec3 t = candidate.turn(a);
                spread_from += length({t.x - from.x, t.y - from.y, t.z - from.z});
                spread_to += length({b.x - to.x, b.y - to.y, b.z - to.z});
            }
            candidate.scale = spread_from > 1e-9f ? spread_to / spread_from : 1.f;
            candidate.offset = {to.x - from.x * candidate.scale, to.y - from.y * candidate.scale,
                to.z - from.z * candidate.scale};
            float error = 0.f;
            for (const auto& [a, b] : pairs)
            {
                const Vec3 p = candidate.point(a);
                error = std::max(error, length({p.x - b.x, p.y - b.y, p.z - b.z}));
            }
            if (error < best_error)
            {
                best_error = error;
                best = candidate;
            }
        }
    } while (std::next_permutation(axis.begin(), axis.end()));
    // An axis permutation's parity times its signs' gives its handedness.
    int inversions = 0;
    for (int i = 0; i < 3; ++i)
    {
        for (int j = i + 1; j < 3; ++j)
        {
            inversions += best.axis[size_t(i)] > best.axis[size_t(j)] ? 1 : 0;
        }
    }
    const float handed = (inversions % 2 ? -1.f : 1.f) * best.sign[0] * best.sign[1] * best.sign[2];
    best.mirrored = handed < 0.f;
    best.turned = best.axis != std::array<int, 3>{0, 1, 2} || best.sign != std::array<float, 3>{1.f, 1.f, 1.f};
    best.identity = !best.turned && std::abs(best.scale - 1.f) < 1e-4f && length(best.offset) < 1e-3f;
    if (best.identity)
    {
        return SkeletonFit{};
    }
    return best;
}

// Whether `rest` is a copy suffix a 3D package adds to a duplicate name: ".001", " #2", "_1", " (2)", "-3". It must
// start with a separator, so "shader_blinn23" never reads as "shader_blinn2".
bool copy_suffix(std::string_view rest)
{
    if (rest.empty())
    {
        return true;
    }
    size_t at = 0;
    while (at < rest.size() && std::string_view(" ._#-").find(rest[at]) != std::string_view::npos)
    {
        ++at;
    }
    if (at == 0)
    {
        return false;
    }
    if (at < rest.size() && rest[at] == '(' && rest.back() == ')')
    {
        rest = rest.substr(at + 1, rest.size() - at - 2);
        at = 0;
    }
    return at < rest.size() && std::all_of(rest.begin() + std::ptrdiff_t(at), rest.end(), [](char c)
    {
        return c >= '0' && c <= '9';
    });
}

// The template material a glTF material name stands for: the same name, else the longest template name it extends with
// a copy suffix (case ignored). Empty when none does.
std::string template_material(const Template& t, const std::string& name)
{
    std::string best;
    const std::string wanted = lower(name);
    for (const Variant& variant : t.variants)
    {
        if (variant.shader == name)
        {
            return variant.shader;
        }
        const std::string known = lower(variant.shader);
        if (wanted.starts_with(known) && copy_suffix(std::string_view(wanted).substr(known.size()))
            && variant.shader.size() > best.size())
        {
            best = variant.shader;
        }
    }
    return best;
}

// Each glTF morph target's template target: the same name, else the same name ignoring case, else the template name it
// contains (a prefixed or suffixed copy); -1 for none.
std::vector<int> template_morphs(const std::vector<std::string>& known, const std::vector<std::string>& names)
{
    std::vector<int> map(names.size(), -1);
    for (size_t morph = 0; morph < names.size(); ++morph)
    {
        const std::string wanted = lower(names[morph]);
        size_t best_length = 0;
        for (size_t index = 0; index < known.size(); ++index)
        {
            const std::string name = lower(known[index]);
            if (known[index] == names[morph] || name == wanted)
            {
                map[morph] = int(index);
                break;
            }
            if (wanted.find(name) != std::string::npos && name.size() > best_length)
            {
                map[morph] = int(index);
                best_length = name.size();
            }
        }
    }
    return map;
}

void analyse(Template& t, u32 node, bool head, u32 variant)
{
    const SerializedImage& image = t.image;
    for (; node != 0; node = image.get(t.tree + node, model_node_field::next))
    {
        const u32 at = t.tree + node;
        const std::string type = image.text(t.tree + image.word(at));
        bool child_head = head;
        u32 child_variant = variant;
        if (type == "ndVertexBuffer")
        {
            if (head || t.branches[0].present)
            {
                throw Error("the template has more than one vertex buffer node");
            }
            t.branches[0].table = at;
            t.branches[0].present = true;
        }
        else if (type == "ndBlendShape")
        {
            if (t.branches[1].present)
            {
                throw Error("the template has more than one blend shape");
            }
            Branch& branch = t.branches[1];
            branch.present = true;
            const u32 targets = t.tree + image.get(at, blendshape_node::targets);
            for (u32 target = 0; target < image.get(at, blendshape_node::target_count); ++target)
            {
                const u32 table = t.tree + image.word(targets, target * 4);
                if (target == 0)
                {
                    branch.table = table;
                }
                else
                {
                    branch.targets.push_back(table);
                }
            }
            child_head = true;
        }
        else if (type == "ndMtxArray")
        {
            Branch& branch = t.branches[head ? 1 : 0];
            if (branch.matrix_node != 0)
            {
                throw Error("a template branch has more than one matrix array");
            }
            branch.matrix_node = at;
            const u32 rows = t.tree + image.get(at, matrix_array_node::rows);
            for (i32 row = 0; row < image.get(at, matrix_array_node::row_count); ++row)
            {
                std::array<u8, 8> bytes{};
                for (u32 b = 0; b < 8; ++b)
                {
                    bytes[b] = image.byte(rows + u32(row) * 8, b);
                }
                branch.rows.push_back(bytes);
            }
        }
        else if (type == "ndShaderParam2")
        {
            Variant out;
            out.head = head;
            out.node = node;
            t.variants.push_back(out);
            child_variant = u32(t.variants.size() - 1);
        }
        else if (type == "ndRigidSkinIdx")
        {
            if (variant == ~0u)
            {
                throw Error("a skin palette sits outside a material");
            }
            const u32 push = image.get(at, model_node_field::child);
            if (push == 0 || image.text(t.tree + image.word(t.tree + push)) != "ndPushBuffer"
                || image.get(t.tree + push, push_buffer_node::header) != 0)
            {
                throw Error("a skin palette's child is not an indexed push buffer");
            }
            Group group;
            group.rigid = at;
            group.push = t.tree + push;
            group.rigid_size = group.push > at && group.push - at <= 0x40 ? group.push - at : 0x28;
            t.variants[variant].groups.push_back(group);
            Branch& branch = t.branches[head ? 1 : 0];
            branch.palette_cap = std::max(branch.palette_cap, image.get(at, rigid_skin_node::index_count));
            continue; // Its push buffer is handled with it.
        }
        const u32 child = image.get(at, model_node_field::child);
        if (child != 0)
        {
            analyse(t, child, child_head, child_variant);
        }
    }
}

struct Influence
{
    std::array<u16, 2> joints{}; // Template joints.
    std::array<float, 2> weights{1.f, 0.f};
    bool blended = false;
};

struct SourceVertex
{
    const GltfPrimitive* primitive = nullptr;
    u32 index = 0;
    Influence influence;
    std::vector<Vec3> morph_positions; // Per template target.
    std::vector<Vec3> morph_normals;
};

struct Triangle
{
    std::array<SourceVertex, 3> corners;
};

// A palette group being built: its rows (palette slots) and its triangles.
struct BuiltGroup
{
    std::vector<u8> palette; // Slot -> matrix row.
    std::vector<u32> indices; // Into the branch's vertices.
};

struct BuiltVertex
{
    Vec3 position{};
    Vec3 normal{};
    Vec2 uv{};
    u32 colour = 0xFFFFFFFF;
    std::array<float, 2> slots{}; // Bone stream: 24 + 3 * slot, 0 for no second bone.
    std::array<float, 2> weights{1.f, 0.f};
    std::vector<Vec3> morph_positions;
    std::vector<Vec3> morph_normals;
};

struct Appender
{
    std::vector<u8>& bytes;

    u32 append(std::span<const u8> data, u32 align)
    {
        while (bytes.size() % align != 0)
        {
            bytes.push_back(0);
        }
        const u32 at = u32(bytes.size());
        bytes.insert(bytes.end(), data.begin(), data.end());
        return at;
    }
    template <class T>
    u32 append_values(const std::vector<T>& values, u32 align)
    {
        return append({reinterpret_cast<const u8*>(values.data()), values.size() * sizeof(T)}, align);
    }
    void set(u32 at, u32 value)
    {
        std::memcpy(bytes.data() + at, &value, 4);
    }
    void set8(u32 at, u8 value)
    {
        bytes[at] = value;
    }
    u32 word(u32 at) const
    {
        return read_u32(bytes, at);
    }
};

void compute_normals(GltfPrimitive& primitive)
{
    for (auto& vertex : primitive.vertices)
    {
        vertex.normal = {};
    }
    for (size_t index = 0; index + 2 < primitive.indices.size(); index += 3)
    {
        GltfVertex& a = primitive.vertices[primitive.indices[index]];
        GltfVertex& b = primitive.vertices[primitive.indices[index + 1]];
        GltfVertex& c = primitive.vertices[primitive.indices[index + 2]];
        const Vec3 u{b.position.x - a.position.x, b.position.y - a.position.y, b.position.z - a.position.z};
        const Vec3 v{c.position.x - a.position.x, c.position.y - a.position.y, c.position.z - a.position.z};
        const Vec3 face{u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x};
        for (GltfVertex* corner : {&a, &b, &c})
        {
            corner->normal = {corner->normal.x + face.x, corner->normal.y + face.y, corner->normal.z + face.z};
        }
    }
    for (auto& vertex : primitive.vertices)
    {
        const float length = std::sqrt(vertex.normal.x * vertex.normal.x + vertex.normal.y * vertex.normal.y
            + vertex.normal.z * vertex.normal.z);
        vertex.normal = length > 0.f ? Vec3{vertex.normal.x / length, vertex.normal.y / length, vertex.normal.z / length}
            : Vec3{0.f, 1.f, 0.f};
    }
}
}

ModelImportResult import_model(const BundleEntry& template_entry, const GltfScene& input)
{
    ModelImportResult result;
    auto note = [&result](const std::string& text)
    {
        if (std::find(result.notes.begin(), result.notes.end(), text) == result.notes.end())
        {
            result.notes.push_back(text);
        }
    };
    const DecodedModel decoded = decode_serialized_model(template_entry);
    Template t{SerializedImage{template_entry.cpu}};
    const SerializedImage& image = t.image;
    for (u32 index = 0; index < image.word(0, 4); ++index)
    {
        const u32 kind = image.word(image.word(0), index * 8);
        const u32 block = image.word(image.word(0), index * 8 + 4);
        if (kind == 0)
        {
            t.tree = image.get(block, model_tree_block_field::tree);
            if (image.get(block, model_tree_block_field::gpu_base) != 0)
            {
                throw Error("the template's vertex data does not start the first GPU chunk");
            }
        }
        else if (kind == 7)
        {
            t.textures = block;
        }
        else if (kind == 0xC || kind == 0xD || kind == 0x12)
        {
            throw Error("the template has GPU data the importer does not move (directory kind "
                + std::to_string(kind) + ")");
        }
    }
    const u32 roots = t.tree + image.get(t.tree, model_tree_field::roots);
    for (u32 root = 0; root < image.get(t.tree, model_tree_field::root_count); ++root)
    {
        analyse(t, image.word(roots, root * 4), false, ~0u);
    }
    for (Variant& variant : t.variants)
    {
        for (const ModelPrimitive& primitive : decoded.primitives)
        {
            if (primitive.material_node == variant.node)
            {
                variant.shader = primitive.material;
                for (const ModelVertex& vertex : primitive.vertices)
                {
                    variant.blended = variant.blended || vertex.joints[0] != vertex.joints[1];
                }
            }
        }
        if (variant.shader.empty())
        {
            const auto material = std::find_if(decoded.materials.begin(), decoded.materials.end(), [&](const auto& m)
            {
                return m.node == variant.node;
            });
            variant.shader = material != decoded.materials.end() ? material->name : "material_" + std::to_string(variant.node);
        }
    }
    for (const auto& line : input.notes)
    {
        note(line);
    }
    // The file's joints by name onto the template skeleton.
    std::vector<int> joint_map(input.joints.size(), -1);
    std::vector<std::pair<Vec3, Vec3>> bind_pairs;
    for (size_t joint = 0; joint < input.joints.size(); ++joint)
    {
        for (size_t known = 0; known < decoded.joints.size(); ++known)
        {
            if (decoded.joints[known].name == input.joints[joint].name)
            {
                joint_map[joint] = int(known);
                bind_pairs.emplace_back(input.joints[joint].bind, decoded.joints[known].world);
                break;
            }
        }
    }
    // A package may have turned the skeleton to its own up axis or units (3ds Max is Z-up): fit the file's bind
    // points to the template's and bring every mesh back with the same turn, scale and offset.
    const SkeletonFit fit = fit_skeleton(bind_pairs);
    std::vector<GltfPrimitive> primitives = input.primitives;
    if (!fit.identity)
    {
        for (GltfPrimitive& primitive : primitives)
        {
            for (GltfVertex& vertex : primitive.vertices)
            {
                vertex.position = fit.point(vertex.position);
                vertex.normal = fit.turn(vertex.normal);
            }
            for (auto& morph : primitive.morph_positions)
            {
                for (Vec3& delta : morph)
                {
                    delta = fit.scaled(delta);
                }
            }
            for (auto& morph : primitive.morph_normals)
            {
                for (Vec3& delta : morph)
                {
                    delta = fit.turn(delta);
                }
            }
            if (fit.mirrored)
            {
                for (size_t index = 0; index + 2 < primitive.indices.size(); index += 3)
                {
                    std::swap(primitive.indices[index + 1], primitive.indices[index + 2]);
                }
            }
        }
        char text[200];
        std::snprintf(text, sizeof(text), "the file's skeleton was %s%s and scaled by %.4g; fitted back to the template's",
            fit.turned ? "turned" : "kept upright", fit.mirrored ? ", mirrored" : "", double(1.f / fit.scale));
        note(text);
    }
    float worst_bind = 0.f;
    std::string worst_joint;
    for (size_t joint = 0; joint < input.joints.size(); ++joint)
    {
        for (size_t known = 0; known < decoded.joints.size(); ++known)
        {
            if (decoded.joints[known].name == input.joints[joint].name)
            {
                const Vec3 a = fit.point(input.joints[joint].bind);
                const Vec3& b = decoded.joints[known].world;
                const float distance = std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y)
                    + (a.z - b.z) * (a.z - b.z));
                if (distance > worst_bind)
                {
                    worst_bind = distance;
                    worst_joint = decoded.joints[known].name;
                }
                break;
            }
        }
    }
    if (worst_bind > 0.01f)
    {
        char text[160];
        std::snprintf(text, sizeof(text), "joint '%s' sits %.3f from the template's bind point; the game keeps the "
            "template skeleton", worst_joint.c_str(), double(worst_bind));
        note(text);
    }
    // New materials: a material name the template does not have becomes a copy of one of its textured materials, with
    // its own colour texture. "name__base" names the copy's base; else the body's is shader_jeans (a neutral grey cel
    // ramp) and the head's shader_blinn24. It is stored as "shader_<name>__<base>", so an export of the result names it
    // with its base and imports as the same material.
    const u32 template_textures = t.textures != 0 ? image.get(t.textures, model_texture_table_field::count) : 0;
    std::vector<std::pair<i32, int>> new_textures; // (texture index, image) for each new material's texture.
    for (GltfPrimitive& primitive : primitives)
    {
        if (!template_material(t, primitive.material).empty())
        {
            continue;
        }
        bool head = lower(primitive.mesh).starts_with("head") || !primitive.morph_positions.empty();
        std::string name = primitive.material;
        for (const char* suffix : {" #", " (", "."})
        {
            const size_t at = name.rfind(suffix);
            if (at != std::string::npos && at > 0 && copy_suffix(std::string_view(name).substr(at)))
            {
                name = name.substr(0, at);
            }
        }
        if (name.empty())
        {
            name = "material";
        }
        const size_t split = name.find("__");
        std::string base = split != std::string::npos ? template_material(t, name.substr(split + 2)) : std::string{};
        // A named base decides the part: its material draws in the body or the head (the blend-shape branch), whatever
        // the mesh's name or shapes say.
        if (!base.empty())
        {
            const auto in = [&](bool part)
            {
                return std::any_of(t.variants.begin(), t.variants.end(), [&](const Variant& v)
                {
                    return v.shader == base && v.base < 0 && v.head == part;
                });
            };
            if (!in(head) && in(!head))
            {
                head = !head;
                if (!head && !primitive.morph_positions.empty())
                {
                    note("material '" + primitive.material + "' draws in the body, where the model has no shapes; its "
                        "shape keys are left out (only the head's materials morph)");
                }
            }
        }
        if (split == std::string::npos)
        {
            for (const char* preferred : head ? std::initializer_list<const char*>{"shader_blinn24", "shader_blinn26"}
                                              : std::initializer_list<const char*>{"shader_jeans", "shader_jumper"})
            {
                if (std::any_of(t.variants.begin(), t.variants.end(), [&](const Variant& v)
                    {
                        return v.shader == preferred && v.head == head && v.base < 0;
                    }))
                {
                    base = preferred;
                    break;
                }
            }
        }
        std::vector<size_t> bases;
        for (size_t index = 0; index < t.variants.size(); ++index)
        {
            if (t.variants[index].shader == base && t.variants[index].base < 0 && t.variants[index].head == head)
            {
                bases.push_back(index);
            }
        }
        const auto base_material = std::find_if(decoded.materials.begin(), decoded.materials.end(), [&](const auto& m)
        {
            return m.name == base;
        });
        if (bases.empty() || base_material == decoded.materials.end() || base_material->colour_texture < 0)
        {
            throw Error("material '" + primitive.material + "' is new, and its base '"
                + (split != std::string::npos ? name.substr(split + 2) : base) + "' is not a textured "
                + (head ? "head" : "body") + " material of " + decoded.aid + "; name it <name>__<base>, for example "
                + (head ? "cap__shader_blinn24" : "overalls__shader_jeans"));
        }
        std::string stored = name.starts_with("shader_") ? name : "shader_" + name;
        if (split == std::string::npos)
        {
            stored += "__" + base;
        }
        primitive.material = stored;
        if (std::any_of(t.variants.begin(), t.variants.end(), [&](const Variant& v)
            {
                return v.shader == stored;
            }))
        {
            continue; // Another primitive made it.
        }
        i32 texture = -1;
        if (primitive.image >= 0 && size_t(primitive.image) < input.images.size())
        {
            texture = i32(template_textures + new_textures.size());
            new_textures.emplace_back(texture, primitive.image);
        }
        else
        {
            note("new material '" + stored + "' has no base colour image; it draws with " + base + "'s texture");
        }
        for (const size_t index : bases)
        {
            Variant copy = t.variants[index];
            copy.shader = stored;
            copy.node = 0;
            copy.groups.clear();
            copy.base = int(index);
            copy.texture = texture;
            t.variants.push_back(copy);
        }
        note("new material '" + stored + "', a copy of " + base);
    }
    // Triangles by variant.
    std::map<size_t, std::vector<Triangle>> by_variant;
    std::set<std::string> collapsed;
    std::set<std::string> missing_morphs;
    for (GltfPrimitive& primitive : primitives)
    {
        if (!primitive.has_normals)
        {
            compute_normals(primitive);
            note("mesh '" + primitive.mesh + "' material '" + primitive.material + "' had no normals; computed them");
        }
        const bool wants_head = lower(primitive.mesh).starts_with("head") || !primitive.morph_positions.empty();
        const std::string shader = template_material(t, primitive.material);
        if (!shader.empty() && shader != primitive.material)
        {
            note("material '" + primitive.material + "' read as '" + shader + "'");
        }
        std::vector<size_t> candidates[2];
        for (size_t index = 0; index < t.variants.size(); ++index)
        {
            if (!shader.empty() && t.variants[index].shader == shader)
            {
                candidates[t.variants[index].head ? 1 : 0].push_back(index);
            }
        }
        const int branch = !candidates[1].empty() && (wants_head || candidates[0].empty()) ? 1 : 0;
        if (candidates[branch].empty())
        {
            std::set<std::string> names;
            for (const Variant& variant : t.variants)
            {
                names.insert(variant.shader);
            }
            std::string list;
            for (const auto& name : names)
            {
                list += (list.empty() ? "" : ", ") + name;
            }
            throw Error("material '" + primitive.material + "' (mesh '" + primitive.mesh + "') is not one of "
                + decoded.aid + "'s: " + list);
        }
        // Its morph targets onto the template's, by name; by order when no name matches and the counts agree (an
        // exporter that does not keep target names).
        std::vector<int> morph_map = template_morphs(decoded.morph_names, primitive.morph_names);
        const bool any_named = std::any_of(morph_map.begin(), morph_map.end(), [](int index)
        {
            return index >= 0;
        });
        if (!any_named && !morph_map.empty() && morph_map.size() == decoded.morph_names.size())
        {
            for (size_t morph = 0; morph < morph_map.size(); ++morph)
            {
                morph_map[morph] = int(morph);
            }
            note("mesh '" + primitive.mesh + "': no morph target name matched, so its " + std::to_string(morph_map.size())
                + " targets were taken in the template's order (" + decoded.morph_names.front() + " first)");
        }
        for (size_t morph = 0; morph < morph_map.size(); ++morph)
        {
            if (morph_map[morph] < 0)
            {
                note("morph target '" + primitive.morph_names[morph] + "' is not one of the template's; left out");
            }
        }
        if (branch == 1)
        {
            for (size_t known = 0; known < decoded.morph_names.size(); ++known)
            {
                const auto& name = decoded.morph_names[known];
                if (std::find(morph_map.begin(), morph_map.end(), int(known)) == morph_map.end())
                {
                    missing_morphs.insert(name);
                }
            }
        }
        size_t rigid_variant = ~size_t(0);
        size_t blended_variant = ~size_t(0);
        for (const size_t index : candidates[branch])
        {
            if (t.variants[index].blended && blended_variant == ~size_t(0))
            {
                blended_variant = index;
            }
            if (!t.variants[index].blended && rigid_variant == ~size_t(0))
            {
                rigid_variant = index;
            }
        }
        const auto influence_of = [&](const GltfVertex& vertex)
        {
            std::array<std::pair<float, int>, 4> list{};
            for (size_t slot = 0; slot < 4; ++slot)
            {
                list[slot] = {vertex.weights[slot], joint_map[vertex.joints[slot]]};
            }
            std::sort(list.begin(), list.end(), [](const auto& a, const auto& b)
            {
                return a.first > b.first;
            });
            Influence out;
            if (list[0].first <= 0.f || list[0].second < 0)
            {
                throw Error("a vertex of mesh '" + primitive.mesh + "' has no weight on a joint the template knows");
            }
            out.joints = {u16(list[0].second), u16(list[0].second)};
            if (list[1].first >= 0.02f && list[1].second >= 0 && list[1].second != list[0].second)
            {
                const float total = list[0].first + list[1].first;
                out.joints[1] = u16(list[1].second);
                out.weights = {list[0].first / total, list[1].first / total};
                out.blended = true;
            }
            return out;
        };
        for (size_t index = 0; index + 2 < primitive.indices.size(); index += 3)
        {
            Triangle triangle;
            bool blended = false;
            for (size_t corner = 0; corner < 3; ++corner)
            {
                SourceVertex& source = triangle.corners[corner];
                source.primitive = &primitive;
                source.index = primitive.indices[index + corner];
                source.influence = influence_of(primitive.vertices[source.index]);
                blended = blended || source.influence.blended;
                if (branch == 1)
                {
                    source.morph_positions.assign(decoded.morph_names.size(), Vec3{});
                    source.morph_normals.assign(decoded.morph_names.size(), Vec3{});
                    for (size_t morph = 0; morph < morph_map.size(); ++morph)
                    {
                        if (morph_map[morph] < 0)
                        {
                            continue;
                        }
                        source.morph_positions[size_t(morph_map[morph])] = primitive.morph_positions[morph][source.index];
                        if (!primitive.morph_normals[morph].empty())
                        {
                            source.morph_normals[size_t(morph_map[morph])] = primitive.morph_normals[morph][source.index];
                        }
                    }
                }
            }
            size_t variant = blended ? blended_variant : rigid_variant;
            if (variant == ~size_t(0))
            {
                variant = blended ? rigid_variant : blended_variant;
                if (blended)
                {
                    // Only a one-bone shader: keep each vertex's strongest joint.
                    collapsed.insert(primitive.material);
                    for (SourceVertex& source : triangle.corners)
                    {
                        source.influence.joints[1] = source.influence.joints[0];
                        source.influence.weights = {1.f, 0.f};
                        source.influence.blended = false;
                    }
                }
            }
            if (decoded.winding_flipped)
            {
                std::swap(triangle.corners[1], triangle.corners[2]);
            }
            by_variant[variant].push_back(std::move(triangle));
        }
    }
    for (const auto& material : collapsed)
    {
        note("material '" + material + "' has only a one-bone shader; its two-bone vertices keep their strongest joint");
    }
    for (const auto& name : missing_morphs)
    {
        note("the glTF has no morph target '" + name + "'; it moves nothing");
    }
    // Palette groups and vertices, per branch.
    std::vector<BuiltVertex> vertices[2];
    std::map<size_t, std::vector<BuiltGroup>> groups;
    for (int branch_index = 0; branch_index < 2; ++branch_index)
    {
        Branch& branch = t.branches[branch_index];
        if (!branch.present)
        {
            continue;
        }
        if (branch.matrix_node == 0 || branch.rows.empty())
        {
            throw Error("a template branch has no matrix array");
        }
        const auto row_of = [&branch](u16 joint) -> u8
        {
            for (size_t row = 0; row < branch.rows.size(); ++row)
            {
                const auto& bytes = branch.rows[row];
                const u16 known = (bytes[5] & 2) != 0 ? u16(bytes[0] | bytes[1] << 8) : bytes[6];
                if (known == joint)
                {
                    return u8(row);
                }
            }
            if (branch.rows.size() >= 255)
            {
                throw Error("a branch needs more than 255 matrix rows");
            }
            std::array<u8, 8> added = branch.rows.front();
            added[0] = u8(joint);
            added[1] = u8(joint >> 8);
            added[5] = u8(added[5] | 2);
            branch.rows.push_back(added);
            return u8(branch.rows.size() - 1);
        };
        for (auto& [variant_index, triangles] : by_variant)
        {
            if (t.variants[variant_index].head != (branch_index == 1))
            {
                continue;
            }
            // Group triangles that share joints: sort by their joint set, then fill palettes greedily.
            std::stable_sort(triangles.begin(), triangles.end(), [](const Triangle& a, const Triangle& b)
            {
                const auto key = [](const Triangle& t)
                {
                    std::array<u16, 6> joints{};
                    for (size_t corner = 0; corner < 3; ++corner)
                    {
                        joints[corner * 2] = t.corners[corner].influence.joints[0];
                        joints[corner * 2 + 1] = t.corners[corner].influence.joints[1];
                    }
                    std::sort(joints.begin(), joints.end());
                    return joints;
                };
                return key(a) < key(b);
            });
            auto& built = groups[variant_index];
            std::set<u16> current;
            std::map<std::tuple<const GltfPrimitive*, u32, u16, u16>, u32> lookup;
            for (const Triangle& triangle : triangles)
            {
                std::set<u16> joints = current;
                for (const SourceVertex& corner : triangle.corners)
                {
                    joints.insert(corner.influence.joints[0]);
                    joints.insert(corner.influence.joints[1]);
                }
                if (built.empty() || joints.size() > branch.palette_cap)
                {
                    built.emplace_back();
                    lookup.clear();
                    current.clear();
                    for (const SourceVertex& corner : triangle.corners)
                    {
                        current.insert(corner.influence.joints[0]);
                        current.insert(corner.influence.joints[1]);
                    }
                    if (current.size() > branch.palette_cap)
                    {
                        throw Error("a triangle needs more joints than a palette holds");
                    }
                }
                else
                {
                    current = joints;
                }
                BuiltGroup& group = built.back();
                for (const SourceVertex& corner : triangle.corners)
                {
                    const auto key = std::tuple{corner.primitive, corner.index, corner.influence.joints[0],
                        corner.influence.joints[1]};
                    auto found = lookup.find(key);
                    if (found == lookup.end())
                    {
                        const auto slot_of = [&](u16 joint) -> u32
                        {
                            const u8 row = row_of(joint);
                            const auto known = std::find(group.palette.begin(), group.palette.end(), row);
                            if (known != group.palette.end())
                            {
                                return u32(known - group.palette.begin());
                            }
                            group.palette.push_back(row);
                            return u32(group.palette.size() - 1);
                        };
                        const GltfVertex& source = corner.primitive->vertices[corner.index];
                        BuiltVertex vertex;
                        vertex.position = source.position;
                        vertex.normal = source.normal;
                        vertex.uv = source.uv;
                        vertex.colour = corner.primitive->has_colours ? source.colour : 0xFFFFFFFF;
                        vertex.slots[0] = 24.f + 3.f * float(slot_of(corner.influence.joints[0]));
                        if (corner.influence.blended)
                        {
                            vertex.slots[1] = 24.f + 3.f * float(slot_of(corner.influence.joints[1]));
                            vertex.weights = corner.influence.weights;
                        }
                        vertex.morph_positions = corner.morph_positions;
                        vertex.morph_normals = corner.morph_normals;
                        vertices[branch_index].push_back(std::move(vertex));
                        found = lookup.emplace(key, u32(vertices[branch_index].size() - 1)).first;
                    }
                    group.indices.push_back(found->second);
                }
            }
        }
        if (vertices[branch_index].size() > 0xFFFF)
        {
            throw Error(std::string(branch_index ? "the head" : "the body") + " has more than 65535 vertices after "
                "splitting into skin palettes");
        }
    }
    // GPU: the first chunk grows by the new vertex streams.
    const auto& chunks = template_entry.gpu_chunks;
    if (chunks.empty())
    {
        throw Error("the template has no GPU chunks");
    }
    std::vector<std::vector<u8>> gpu;
    for (const auto& chunk : chunks)
    {
        gpu.emplace_back(template_entry.gpu.begin() + chunk.offset, template_entry.gpu.begin() + chunk.offset + chunk.size);
    }
    const u32 first_chunk = u32(gpu[0].size());
    Appender gpu_out{gpu[0]};
    std::vector<u8> cpu(template_entry.cpu.begin(), template_entry.cpu.end());
    Appender cpu_out{cpu};
    const u32 tree = t.tree;
    const auto tree_relative = [tree](u32 absolute)
    {
        return absolute - tree;
    };
    // Vertex streams by the template records' kinds.
    struct WrittenStream
    {
        u32 data = 0;
        u32 size = 0;
    };
    for (int branch_index = 0; branch_index < 2; ++branch_index)
    {
        Branch& branch = t.branches[branch_index];
        if (!branch.present)
        {
            continue;
        }
        const auto& list = vertices[branch_index];
        std::map<u8, WrittenStream> base_streams;
        const u32 records = tree + image.get(branch.table, vertex_buffer_node::buffers);
        const u32 count = image.get(branch.table, vertex_buffer_node::buffer_count);
        for (u32 index = 0; index < count; ++index)
        {
            const u32 record = records + index * model_vertex_buffer_field::kSize;
            const u8 stride = image.byte(record);
            const u8 kind = image.byte(record, 1);
            if (image.word(record, 0x14) == 0)
            {
                continue; // Unused in the template (and the blend placeholders): keep it empty.
            }
            std::vector<u8> data(list.size() * stride, 0);
            for (size_t vertex = 0; vertex < list.size(); ++vertex)
            {
                const BuiltVertex& v = list[vertex];
                u8* out = data.data() + vertex * stride;
                const auto floats = [&](std::initializer_list<float> values)
                {
                    size_t component = 0;
                    for (const float value : values)
                    {
                        if ((component + 1) * 4 <= stride)
                        {
                            std::memcpy(out + component * 4, &value, 4);
                        }
                        ++component;
                    }
                };
                switch (kind)
                {
                case 9:
                    floats({v.position.x, v.position.y, v.position.z});
                    break;
                case 0xA:
                    floats({v.normal.x, v.normal.y, v.normal.z});
                    break;
                case 0xD:
                case 0xE:
                case 0xF:
                case 0x10:
                    floats({v.uv.u, v.uv.v});
                    break;
                case 0:
                    floats({v.slots[0], v.slots[1]});
                    break;
                case 8:
                    floats({v.weights[0], v.weights[1]});
                    break;
                case 0xB:
                    std::memcpy(out, &v.colour, std::min<size_t>(4, stride));
                    break;
                case 0xC:
                    break; // Specular: zero.
                default:
                    throw Error("the template has a vertex stream of kind " + std::to_string(kind)
                        + " the importer does not write");
                }
            }
            const u32 at = gpu_out.append(data, kGpuAlign);
            base_streams[kind] = {at, u32(data.size())};
            std::memcpy(cpu.data() + record + 0x10, &at, 4);
            const u32 size = u32(data.size());
            std::memcpy(cpu.data() + record + 0x14, &size, 4);
        }
        for (size_t target = 0; target < branch.targets.size(); ++target)
        {
            const u32 table = branch.targets[target];
            const u32 target_records = tree + image.get(table, vertex_buffer_node::buffers);
            for (u32 index = 0; index < image.get(table, vertex_buffer_node::buffer_count); ++index)
            {
                const u32 record = target_records + index * model_vertex_buffer_field::kSize;
                const u8 stride = image.byte(record);
                const u8 kind = image.byte(record, 1);
                if (image.word(record, 0x14) == 0)
                {
                    continue;
                }
                WrittenStream written;
                if (kind == 9 || kind == 0xA)
                {
                    // Deltas from the base (0x00157650 adds weight * target).
                    std::vector<u8> data(list.size() * stride, 0);
                    for (size_t vertex = 0; vertex < list.size(); ++vertex)
                    {
                        const auto& deltas = kind == 9 ? list[vertex].morph_positions : list[vertex].morph_normals;
                        const Vec3 delta = target < deltas.size() ? deltas[target] : Vec3{};
                        const float values[3]{delta.x, delta.y, delta.z};
                        std::memcpy(data.data() + vertex * stride, values, std::min<size_t>(12, stride));
                    }
                    written = {gpu_out.append(data, kGpuAlign), u32(data.size())};
                }
                else
                {
                    const auto base = base_streams.find(kind);
                    if (base == base_streams.end())
                    {
                        throw Error("a blend target's stream of kind " + std::to_string(kind) + " has no base stream");
                    }
                    written = base->second; // The target's copy of the base data.
                }
                std::memcpy(cpu.data() + record + 0x10, &written.data, 4);
                std::memcpy(cpu.data() + record + 0x14, &written.size, 4);
            }
        }
        // Matrix rows (extended when the new skin uses joints the template's palettes did not).
        if (branch.rows.size() != size_t(image.get(branch.matrix_node, matrix_array_node::row_count)))
        {
            std::vector<u8> rows;
            for (const auto& row : branch.rows)
            {
                rows.insert(rows.end(), row.begin(), row.end());
            }
            const u32 at = cpu_out.append(rows, 16);
            cpu_out.set(branch.matrix_node + matrix_array_node::rows.offset, tree_relative(at));
            cpu_out.set(branch.matrix_node + matrix_array_node::row_count.offset, u32(branch.rows.size()));
            note(std::string(branch_index ? "head" : "body") + " matrix array grew to "
                + std::to_string(branch.rows.size()) + " rows for joints the template did not skin");
        }
    }
    // Palette groups: reuse the variant's template nodes, append copies for more, unlink the rest.
    u32 instance_size = image.word(tree);
    size_t total_groups = 0;
    size_t total_triangles = 0;
    std::vector<std::pair<std::string, u32>> clone_names; // A new material's stored name and its node, tree-relative.
    for (size_t variant_index = 0; variant_index < t.variants.size(); ++variant_index)
    {
        Variant& variant = t.variants[variant_index];
        const Variant* base = variant.base >= 0 ? &t.variants[size_t(variant.base)] : nullptr;
        if (variant.groups.empty() && (base == nullptr || base->groups.empty()))
        {
            continue;
        }
        u32 clone_node = 0;
        if (base != nullptr)
        {
            // The new material's node: a copy of the base's ndShaderParam2 beside it, with copies of its material
            // records and of the arrays the loader rewrites in place (0x0015ACB0 relocates the entries' names), and
            // the colour stage on the new texture. Colours, constants and the name stay shared.
            const u32 base_node = tree + base->node;
            clone_node = cpu_out.append(std::span<const u8>(template_entry.cpu.data() + base_node, 0x28), 4);
            u32 storage = 0xC;
            for (const u32 field : {u32(shader_node::primary.offset), u32(shader_node::secondary.offset)})
            {
                const u32 material = image.word(base_node, field);
                if (material == 0)
                {
                    continue;
                }
                const u32 from = tree + material;
                const u32 copy = cpu_out.append(std::span<const u8>(template_entry.cpu.data() + from, 0x48), 4);
                const u32 stage_count = image.get(from, model_material_field::stage_count);
                const u32 stages = image.get(from, model_material_field::stages);
                u32 stages_copy = 0;
                if (stages != 0 && stage_count != 0)
                {
                    stages_copy = cpu_out.append(std::span<const u8>(template_entry.cpu.data() + tree + stages,
                        stage_count * model_material_stage_field::kSize), 4);
                    cpu_out.set(copy + model_material_field::stages.offset, tree_relative(stages_copy));
                }
                const u32 entry_count = image.get(from, model_material_field::entry_count);
                const u32 entries = image.get(from, model_material_field::entries);
                if (entries != 0 && entry_count != 0)
                {
                    const u32 entries_copy = cpu_out.append(std::span<const u8>(template_entry.cpu.data() + tree + entries,
                        entry_count * model_material_entry_field::kSize), 4);
                    cpu_out.set(copy + model_material_field::entries.offset, tree_relative(entries_copy));
                    for (u32 entry = 0; entry < entry_count && variant.texture >= 0 && stages_copy != 0; ++entry)
                    {
                        const u32 record = tree + entries + entry * model_material_entry_field::kSize;
                        const i32 stage = image.get(record, model_material_entry_field::value);
                        if (image.text(tree + image.get(record, model_material_entry_field::name)) == "colour0"
                            && stage >= 0 && u32(stage) < stage_count)
                        {
                            cpu_out.set(stages_copy + u32(stage) * model_material_stage_field::kSize
                                + model_material_stage_field::texture.offset, u32(variant.texture));
                        }
                    }
                }
                cpu_out.set(clone_node + field, tree_relative(copy));
                storage += image.get(from, model_material_field::instance_size);
            }
            cpu_out.set(clone_node + model_node_field::instance_offset.offset, instance_size);
            instance_size += (storage + 3) & ~3u;
            // Linked after the base, which the loop walks with the vertex shader's other parameters.
            cpu_out.set(clone_node + model_node_field::next.offset, cpu_out.word(base_node + model_node_field::next.offset));
            cpu_out.set(base_node + model_node_field::next.offset, tree_relative(clone_node));
            cpu_out.set(clone_node + model_node_field::unk_1C.offset, 0);
            variant.node = tree_relative(clone_node);
            clone_names.emplace_back(variant.shader, variant.node);
        }
        const auto built = groups.find(variant_index);
        const std::vector<BuiltGroup> empty;
        const std::vector<BuiltGroup>& list = built != groups.end() ? built->second : empty;
        std::vector<Group> nodes;
        for (size_t index = 0; index < std::max<size_t>(list.size(), 1); ++index)
        {
            if (index < variant.groups.size())
            {
                nodes.push_back(variant.groups[index]);
                continue;
            }
            // A copy of the variant's (or its base's) first group, with its own instance storage.
            const Group& source = base != nullptr ? base->groups.front() : variant.groups.front();
            Group copy;
            copy.rigid = cpu_out.append(std::span<const u8>(template_entry.cpu.data() + source.rigid, source.rigid_size), 4);
            copy.push = cpu_out.append(std::span<const u8>(template_entry.cpu.data() + source.push, 0x40), 4);
            copy.rigid_size = source.rigid_size;
            cpu_out.set(copy.rigid + model_node_field::instance_offset.offset, instance_size);
            instance_size += 4;
            cpu_out.set(copy.push + model_node_field::instance_offset.offset, instance_size);
            instance_size += 4;
            nodes.push_back(copy);
        }
        for (size_t index = 0; index < nodes.size(); ++index)
        {
            const Group& node = nodes[index];
            cpu_out.set(node.rigid + model_node_field::child.offset, tree_relative(node.push));
            cpu_out.set(node.rigid + model_node_field::next.offset,
                index + 1 < nodes.size() ? tree_relative(nodes[index + 1].rigid) : 0);
            cpu_out.set(node.rigid + model_node_field::unk_1C.offset, 0);
            cpu_out.set(node.push + model_node_field::child.offset, 0);
            cpu_out.set(node.push + model_node_field::next.offset, 0);
            cpu_out.set(node.push + model_node_field::unk_1C.offset, 0);
            if (index < list.size())
            {
                const BuiltGroup& group = list[index];
                std::vector<u8> palette = group.palette;
                const u32 palette_at = cpu_out.append(palette, 4);
                cpu_out.set(node.rigid + rigid_skin_node::indices.offset, tree_relative(palette_at));
                cpu_out.set(node.rigid + rigid_skin_node::index_count.offset, u32(palette.size()));
                std::vector<u16> indices;
                for (const u32 index_value : group.indices)
                {
                    indices.push_back(u16(index_value));
                }
                const u32 indices_at = cpu_out.append_values(indices, 4);
                const std::vector<u32> draw_indices{tree_relative(indices_at)};
                const std::vector<u32> draw_primitives{5}; // Triangle lists.
                const std::vector<u32> draw_counts{u32(indices.size())};
                cpu_out.set(node.push + push_buffer_node::draw_indices.offset,
                    tree_relative(cpu_out.append_values(draw_indices, 4)));
                cpu_out.set(node.push + push_buffer_node::draw_primitives.offset,
                    tree_relative(cpu_out.append_values(draw_primitives, 4)));
                cpu_out.set(node.push + push_buffer_node::draw_counts.offset,
                    tree_relative(cpu_out.append_values(draw_counts, 4)));
                cpu_out.set(node.push + push_buffer_node::draw_count.offset, 1);
                total_triangles += indices.size() / 3;
            }
            else
            {
                // The variant draws nothing now: keep its first palette node, with no slots and no draws.
                cpu_out.set(node.rigid + rigid_skin_node::index_count.offset, 0);
                cpu_out.set(node.push + push_buffer_node::draw_count.offset, 0);
            }
        }
        if (clone_node != 0)
        {
            cpu_out.set(clone_node + model_node_field::child.offset, tree_relative(nodes.front().rigid));
        }
        total_groups += list.size();
    }
    cpu_out.set(tree, instance_size);
    // Name entries for the new materials, so the model's views and an export name them (names starting "shader_" hold
    // a word with the node's offset).
    if (!clone_names.empty())
    {
        const u32 names = image.get(tree, model_tree_field::names);
        if (names == 0)
        {
            note("the template has no name table; new materials export unnamed");
        }
        else
        {
            const u32 count = image.word(tree + names);
            const u32 old_entries = tree + image.word(tree + names, 4);
            u32 third = 0;
            for (u32 entry = 0; entry < count; ++entry)
            {
                if (image.text(tree + image.word(old_entries + entry * 0xC)).starts_with("shader_"))
                {
                    third = image.word(old_entries + entry * 0xC, 8);
                    break;
                }
            }
            std::vector<u8> entries(template_entry.cpu.begin() + old_entries,
                template_entry.cpu.begin() + old_entries + count * 0xC);
            for (const auto& [name, node] : clone_names)
            {
                const u32 text = cpu_out.append(std::span<const u8>(reinterpret_cast<const u8*>(name.c_str()),
                    name.size() + 1), 4);
                const std::vector<u32> value{node};
                const u32 word = cpu_out.append_values(value, 4);
                const u32 record[3]{tree_relative(text), tree_relative(word), third};
                entries.insert(entries.end(), reinterpret_cast<const u8*>(record), reinterpret_cast<const u8*>(record) + 12);
            }
            const u32 at = cpu_out.append(entries, 4);
            cpu_out.set(tree + names, count + u32(clone_names.size()));
            cpu_out.set(tree + names + 4, tree_relative(at));
        }
    }
    // Texture records past the first chunk move with its growth (0x00052710 resolves them across the chunks).
    const u32 growth = u32(gpu[0].size()) - first_chunk;
    if (t.textures != 0 && growth != 0)
    {
        for (u32 index = 0; index < image.get(t.textures, model_texture_table_field::count); ++index)
        {
            const u32 record = image.word(image.get(t.textures, model_texture_table_field::textures), index * 4);
            const u32 data = image.get(record, model_texture_field::data);
            if (image.get(record, model_texture_field::frame_size) != 0 && data >= first_chunk)
            {
                cpu_out.set(record + model_texture_field::data.offset, data + growth);
            }
        }
    }
    // Texture data that does not fit where the template's was goes in a GPU chunk of its own, after the others. A
    // texture's size is its image's, rounded to a power of two from 4 to 1024; postload (0x000534A0) builds the D3D
    // header from the record's width, height, levels and format.
    std::vector<u8> extra;
    u32 extra_base = 0;
    for (const auto& chunk : gpu)
    {
        extra_base += u32(chunk.size());
    }
    Appender extra_out{extra};
    const auto power_of_two = [](int size)
    {
        int best = 4;
        for (int candidate = 4; candidate <= 1024; candidate *= 2)
        {
            if (std::abs(candidate - size) < std::abs(best - size))
            {
                best = candidate;
            }
        }
        return best;
    };
    const auto texture_record = [&](i32 index) -> u32
    {
        return image.word(image.get(t.textures, model_texture_table_field::textures), u32(index) * 4);
    };
    // Points the record at new DXT1 data of its own size.
    const auto place = [&](u32 record, const RgbaImage& picture)
    {
        const auto encoded = encode_dxt1(picture);
        const u32 at = extra_base + extra_out.append(encoded, kGpuAlign);
        cpu_out.set(record + model_texture_field::data.offset, at);
        cpu_out.set(record + model_texture_field::frame_size.offset, u32(encoded.size()));
        const u32 size = u32(picture.width) | u32(picture.height) << 16;
        cpu_out.set(record + model_texture_field::width.offset, size); // Width at +8, height at +0xA.
        cpu_out.set8(record + model_texture_field::levels.offset, 1);
    };
    // Records the import adds; index template_textures + position.
    std::vector<u32> added;
    // A copy of a texture record (with its frame headers) for new data.
    const auto clone_record = [&](i32 from) -> u32
    {
        const u32 source = texture_record(from);
        if (XboxFormat(image.get(source, model_texture_field::format)) != XboxFormat::DXT1)
        {
            throw Error("texture " + std::to_string(from) + " is not DXT1, so it cannot be copied for a new texture");
        }
        const u32 frames = image.get(source, model_texture_field::frames);
        const u32 length = frames + std::max<u32>(image.get(source, model_texture_field::frame_count), 1)
            * model_texture_field::kFrameHeaderSize;
        const u32 record = cpu_out.append(std::span<const u8>(template_entry.cpu.data() + source, length), 16);
        cpu_out.set8(record + model_texture_field::frame_count.offset, 1);
        return record;
    };
    // The new materials' textures: copies of their base's colour texture record, on their own data.
    for (const auto& [texture_index, picture_index] : new_textures)
    {
        const Variant& clone = *std::find_if(t.variants.begin(), t.variants.end(), [&](const Variant& v)
        {
            return v.texture == texture_index;
        });
        const std::string& base_name = t.variants[size_t(clone.base)].shader;
        const auto base = std::find_if(decoded.materials.begin(), decoded.materials.end(), [&](const auto& m)
        {
            return m.name == base_name && m.colour_texture >= 0;
        });
        const u32 record = clone_record(base->colour_texture);
        const RgbaImage& picture = input.images[size_t(picture_index)];
        const int width = power_of_two(picture.width);
        const int height = power_of_two(picture.height);
        place(record, resize_image(picture, width, height));
        added.push_back(record);
        note("material '" + clone.shader + "' takes its new texture " + std::to_string(texture_index) + " ("
            + std::to_string(width) + "x" + std::to_string(height) + " DXT1)");
    }
    // Points every variant of a template material at another colour texture.
    const auto repoint = [&](const std::string& shader, i32 texture)
    {
        for (const Variant& variant : t.variants)
        {
            if (variant.shader != shader || variant.base >= 0)
            {
                continue;
            }
            for (const u32 field : {u32(shader_node::primary.offset), u32(shader_node::secondary.offset)})
            {
                const u32 material = image.word(tree + variant.node, field);
                if (material == 0)
                {
                    continue;
                }
                const u32 from = tree + material;
                const u32 stages = image.get(from, model_material_field::stages);
                const u32 entries = image.get(from, model_material_field::entries);
                for (u32 entry = 0; entries != 0 && entry < image.get(from, model_material_field::entry_count); ++entry)
                {
                    const u32 record = tree + entries + entry * model_material_entry_field::kSize;
                    const i32 stage = image.get(record, model_material_entry_field::value);
                    if (stages != 0 && image.text(tree + image.get(record, model_material_entry_field::name)) == "colour0"
                        && stage >= 0 && u32(stage) < image.get(from, model_material_field::stage_count))
                    {
                        cpu_out.set(tree + stages + u32(stage) * model_material_stage_field::kSize
                            + model_material_stage_field::texture.offset, u32(texture));
                    }
                }
            }
        }
    };
    std::set<i32> replaced;
    for (const GltfPrimitive& primitive : primitives)
    {
        if (primitive.image < 0 || size_t(primitive.image) >= input.images.size())
        {
            continue;
        }
        const std::string shader = template_material(t, primitive.material);
        const auto material = std::find_if(decoded.materials.begin(), decoded.materials.end(), [&](const auto& m)
        {
            return m.name == shader && m.colour_texture >= 0;
        });
        if (material == decoded.materials.end() || replaced.contains(material->colour_texture)
            || size_t(material->colour_texture) >= decoded.textures.size()
            || std::any_of(t.variants.begin(), t.variants.end(), [&](const Variant& v)
            {
                return v.shader == shader && v.base >= 0;
            }))
        {
            continue; // A new material's image is written above.
        }
        const ModelTextureInfo& texture = decoded.textures[size_t(material->colour_texture)];
        const RgbaImage& source = input.images[size_t(primitive.image)];
        if (resize_image(source, texture.width, texture.height).bgra == texture.image.bgra)
        {
            continue; // Unchanged: keep the disc's compressed data.
        }
        if (XboxFormat(texture.format) != XboxFormat::DXT1)
        {
            note("texture " + std::to_string(texture.index) + " of '" + shader + "' is "
                + xbox_format_name(XboxFormat(texture.format)) + ", which the importer does not encode; kept");
            continue;
        }
        const u32 record = texture_record(i32(texture.index));
        int width = power_of_two(source.width);
        int height = power_of_two(source.height);
        if (image.get(record, model_texture_field::frame_count) > 1)
        {
            width = texture.width; // An animated texture keeps its frames' size.
            height = texture.height;
        }
        const RgbaImage resized = resize_image(source, width, height);
        replaced.insert(material->colour_texture);
        // A texture another material also uses stays theirs: this material gets a copy of its own.
        std::string sharers;
        for (const ModelMaterialInfo& other : decoded.materials)
        {
            if (other.name != shader && (other.colour_texture == i32(texture.index)
                || other.celmap_texture == i32(texture.index) || other.lightmap_texture == i32(texture.index))
                && sharers.find(other.name) == std::string::npos)
            {
                sharers += (sharers.empty() ? "" : ", ") + other.name;
            }
        }
        if (!sharers.empty())
        {
            const i32 index = i32(template_textures + added.size());
            const u32 copy = clone_record(i32(texture.index));
            place(copy, resized);
            added.push_back(copy);
            repoint(shader, index);
            note("material '" + shader + "' takes a new texture " + std::to_string(index) + " (" + std::to_string(width)
                + "x" + std::to_string(height) + " DXT1); texture " + std::to_string(texture.index) + " stays as it is for "
                + sharers);
            continue;
        }
        if (width == texture.width && height == texture.height)
        {
            // The same size: over the template's data, where it is.
            const auto encoded = encode_dxt1(resized);
            u32 start = 0;
            for (size_t chunk = 0; chunk < chunks.size(); ++chunk)
            {
                const u32 size = chunks[chunk].size;
                if (texture.data >= start && texture.data - start + encoded.size() <= size)
                {
                    std::memcpy(gpu[chunk].data() + (texture.data - start), encoded.data(), encoded.size());
                    break;
                }
                start += size;
            }
        }
        else
        {
            place(record, resized);
        }
        note("material '" + shader + "' takes a new texture (" + std::to_string(width) + "x" + std::to_string(height)
            + " DXT1)");
    }
    if (!added.empty())
    {
        if (t.textures == 0)
        {
            throw Error("the template has no texture table for new textures");
        }
        std::vector<u32> pointers;
        for (u32 index = 0; index < template_textures; ++index)
        {
            pointers.push_back(texture_record(i32(index)));
        }
        pointers.insert(pointers.end(), added.begin(), added.end());
        const u32 list = cpu_out.append_values(pointers, 4);
        cpu_out.set(t.textures + model_texture_table_field::count.offset, u32(pointers.size()));
        cpu_out.set(t.textures + model_texture_table_field::textures.offset, list);
    }
    if (!extra.empty())
    {
        while (extra.size() % kGpuAlign != 0)
        {
            extra.push_back(0);
        }
        gpu.push_back(std::move(extra));
    }
    // The loose resource: header, GPU chunk table and chunks, then the CPU block.
    std::vector<u8> table;
    const auto put = [](std::vector<u8>& out, u32 value)
    {
        const u8* bytes = reinterpret_cast<const u8*>(&value);
        out.insert(out.end(), bytes, bytes + 4);
    };
    const u32 table_size = 8 + 8 * u32(gpu.size());
    put(table, table_size);
    put(table, u32(gpu.size()));
    u32 offset = table_size;
    for (const auto& chunk : gpu)
    {
        put(table, offset);
        put(table, u32(chunk.size()));
        offset += u32(chunk.size());
    }
    std::vector<u8> resource_chunk = table;
    for (const auto& chunk : gpu)
    {
        resource_chunk.insert(resource_chunk.end(), chunk.begin(), chunk.end());
    }
    std::vector<u8>& out = result.resource;
    put(out, u32(ResourceType::Model));
    put(out, template_entry.header.unknown);
    put(out, kResourceMagic);
    put(out, 2);
    put(out, 32 + u32(resource_chunk.size()));
    put(out, u32(cpu.size()));
    put(out, 32);
    put(out, u32(resource_chunk.size()));
    out.insert(out.end(), resource_chunk.begin(), resource_chunk.end());
    out.insert(out.end(), cpu.begin(), cpu.end());
    char summary[200];
    std::snprintf(summary, sizeof(summary), "%zu body and %zu head vertices, %zu triangles in %zu skin palettes; GPU "
        "chunk 0 grew by %u bytes", vertices[0].size(), vertices[1].size(), total_triangles, total_groups, growth);
    note(summary);
    return result;
}
}
#endif
