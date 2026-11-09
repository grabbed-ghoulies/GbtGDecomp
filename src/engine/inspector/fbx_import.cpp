#if defined(GBTG_INSPECTOR_APP) && !defined(GBTG_RETAIL_GAME_ONLY)
#include "fbx_import.h"
#include "fbx_document.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <map>
#include <unordered_map>

namespace gbtg
{
namespace
{
// Column-major 4x4 (FBX stores the translation at 12..14).
using Matrix = std::array<double, 16>;

Matrix identity()
{
    return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
}

Matrix multiply(const Matrix& a, const Matrix& b)
{
    Matrix out{};
    for (int column = 0; column < 4; ++column)
    {
        for (int row = 0; row < 4; ++row)
        {
            double sum = 0.0;
            for (int k = 0; k < 4; ++k)
            {
                sum += a[size_t(k * 4 + row)] * b[size_t(column * 4 + k)];
            }
            out[size_t(column * 4 + row)] = sum;
        }
    }
    return out;
}

Vec3 point(const Matrix& m, const Vec3& v)
{
    return {float(m[0] * v.x + m[4] * v.y + m[8] * v.z + m[12]), float(m[1] * v.x + m[5] * v.y + m[9] * v.z + m[13]),
        float(m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14])};
}

Vec3 direction(const Matrix& m, const Vec3& v)
{
    return {float(m[0] * v.x + m[4] * v.y + m[8] * v.z), float(m[1] * v.x + m[5] * v.y + m[9] * v.z),
        float(m[2] * v.x + m[6] * v.y + m[10] * v.z)};
}

Vec3 point3(const Matrix& m, size_t index, const std::vector<double>& control)
{
    return point(m, {float(control[index * 3]), float(control[index * 3 + 1]), float(control[index * 3 + 2])});
}

Matrix rotation_xyz(double x, double y, double z)
{
    // Euler XYZ (FBX's default order): X first, then Y, then Z.
    const double cx = std::cos(x), sx = std::sin(x), cy = std::cos(y), sy = std::sin(y), cz = std::cos(z),
                 sz = std::sin(z);
    const Matrix rx{1, 0, 0, 0, 0, cx, sx, 0, 0, -sx, cx, 0, 0, 0, 0, 1};
    const Matrix ry{cy, 0, -sy, 0, 0, 1, 0, 0, sy, 0, cy, 0, 0, 0, 0, 1};
    const Matrix rz{cz, sz, 0, 0, -sz, cz, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    return multiply(rz, multiply(ry, rx));
}

Matrix to_matrix(const std::vector<double>& values)
{
    if (values.size() != 16)
    {
        throw Error("an FBX matrix does not have 16 values");
    }
    Matrix m;
    std::copy(values.begin(), values.end(), m.begin());
    return m;
}

std::string strip(const std::string& name)
{
    // "name\0\1Class" -> "name"; ASCII files and some writers use "Class::name".
    const size_t separator = name.find(std::string("\x00\x01", 2));
    if (separator != std::string::npos)
    {
        return name.substr(0, separator);
    }
    const size_t colons = name.find("::");
    return colons != std::string::npos ? name.substr(colons + 2) : name;
}

struct Object
{
    const FbxNode* node = nullptr;
    std::string name;
    std::string kind; // properties[2]: "Mesh", "LimbNode", "Skin", "Cluster", "BlendShape", ...
};

struct Link
{
    std::int64_t child = 0;
    std::int64_t parent = 0;
    std::string property;
};

struct Document
{
    std::unordered_map<std::int64_t, Object> objects;
    std::vector<Link> links;

    std::vector<std::int64_t> children(std::int64_t parent, const char* type = nullptr) const
    {
        std::vector<std::int64_t> out;
        for (const Link& link : links)
        {
            if (link.parent == parent)
            {
                const auto found = objects.find(link.child);
                if (found != objects.end() && (type == nullptr || found->second.node->name == type))
                {
                    out.push_back(link.child);
                }
            }
        }
        return out;
    }
    std::vector<std::int64_t> parents(std::int64_t child, const char* type = nullptr) const
    {
        std::vector<std::int64_t> out;
        for (const Link& link : links)
        {
            if (link.child == child)
            {
                const auto found = objects.find(link.parent);
                if (found != objects.end() && (type == nullptr || found->second.node->name == type))
                {
                    out.push_back(link.parent);
                }
            }
        }
        return out;
    }
    const FbxNode& node(std::int64_t id) const
    {
        return *objects.at(id).node;
    }

    Vec3 vector_property(const FbxNode& node, const char* name, const Vec3& fallback) const
    {
        const FbxNode* p = node.p(name);
        if (p == nullptr || p->properties.size() < 7)
        {
            return fallback;
        }
        return {float(p->properties[4].number()), float(p->properties[5].number()), float(p->properties[6].number())};
    }

    // A model's global transform from its local translation, (pre-)rotation and scale and its parents'.
    Matrix global(std::int64_t model, int depth = 0) const
    {
        if (depth > 256)
        {
            throw Error("FBX model parents loop");
        }
        const FbxNode& node = this->node(model);
        const Vec3 t = vector_property(node, "Lcl Translation", {});
        const Vec3 r = vector_property(node, "Lcl Rotation", {});
        const Vec3 pre = vector_property(node, "PreRotation", {});
        const Vec3 s = vector_property(node, "Lcl Scaling", {1.f, 1.f, 1.f});
        constexpr double kRadians = 3.14159265358979323846 / 180.0;
        Matrix local = multiply(rotation_xyz(pre.x * kRadians, pre.y * kRadians, pre.z * kRadians),
            rotation_xyz(r.x * kRadians, r.y * kRadians, r.z * kRadians));
        for (int axis = 0; axis < 3; ++axis)
        {
            const double scale = axis == 0 ? s.x : axis == 1 ? s.y : s.z;
            for (int row = 0; row < 3; ++row)
            {
                local[size_t(axis * 4 + row)] *= scale;
            }
        }
        local[12] = t.x;
        local[13] = t.y;
        local[14] = t.z;
        for (const std::int64_t parent : parents(model, "Model"))
        {
            return multiply(global(parent, depth + 1), local);
        }
        return local;
    }
};

const FbxProperty* first_property(const FbxNode* node)
{
    return node != nullptr && !node->properties.empty() ? &node->properties[0] : nullptr;
}

std::vector<double> numbers(const FbxNode* parent, const char* name)
{
    const FbxNode* child = parent != nullptr ? parent->find(name) : nullptr;
    const FbxProperty* p = first_property(child);
    return p != nullptr ? p->numbers() : std::vector<double>{};
}

std::string text(const FbxNode* parent, const char* name)
{
    const FbxNode* child = parent != nullptr ? parent->find(name) : nullptr;
    const FbxProperty* p = first_property(child);
    return p != nullptr ? p->text : std::string{};
}

// A layer element's value index for one polygon vertex.
struct Layer
{
    const FbxNode* node = nullptr;
    std::vector<double> values;
    std::vector<double> indices;
    std::string mapping;
    bool indexed = false;
    int width = 0;

    Layer(const FbxNode* geometry, const char* element, const char* data, const char* index, int components)
        : width(components)
    {
        node = geometry->find(element);
        if (node == nullptr)
        {
            return;
        }
        values = numbers(node, data);
        indices = numbers(node, index);
        mapping = text(node, "MappingInformationType");
        indexed = text(node, "ReferenceInformationType") != "Direct";
    }
    bool present() const
    {
        return node != nullptr && !values.empty();
    }
    // The value's first component index for polygon vertex `corner` of control point `control` in polygon `polygon`.
    size_t at(size_t corner, size_t control, size_t polygon) const
    {
        size_t slot = mapping == "ByPolygonVertex" ? corner
            : mapping == "ByPolygon" ? polygon
            : mapping == "AllSame" ? 0 : control; // ByVertice / ByVertex / ByControlPoint.
        if (indexed && !indices.empty())
        {
            if (slot >= indices.size())
            {
                throw Error("an FBX layer index is out of range");
            }
            slot = size_t(indices[slot]);
        }
        const size_t first = slot * size_t(width);
        if (first + size_t(width) > values.size())
        {
            throw Error("an FBX layer value is out of range");
        }
        return first;
    }
};
}

GltfScene read_fbx_scene(const std::filesystem::path& path)
{
    const std::vector<u8> file = read_file(path);
    const std::vector<FbxNode> top = read_fbx(file);
    Document document;
    GltfScene scene;
    for (const FbxNode& section : top)
    {
        if (section.name == "Objects")
        {
            for (const FbxNode& object : section.children)
            {
                if (object.properties.size() >= 3)
                {
                    document.objects[std::int64_t(object.properties[0].number())] = Object{&object,
                        strip(object.properties[1].text), object.properties[2].text};
                }
            }
        }
        else if (section.name == "Connections")
        {
            for (const FbxNode& c : section.children)
            {
                if (c.properties.size() >= 3)
                {
                    document.links.push_back(Link{std::int64_t(c.properties[1].number()),
                        std::int64_t(c.properties[2].number()), c.properties.size() > 3 ? c.properties[3].text : ""});
                }
            }
        }
    }
    // Bind matrices a BindPose records, by node.
    std::unordered_map<std::int64_t, Matrix> pose;
    for (const auto& [object_id, object] : document.objects)
    {
        if (object.node->name == "Pose" && object.kind == "BindPose")
        {
            for (const FbxNode* entry : object.node->all("PoseNode"))
            {
                const auto node_id = numbers(entry, "Node");
                const auto matrix = numbers(entry, "Matrix");
                if (node_id.size() == 1 && matrix.size() == 16)
                {
                    pose[std::int64_t(node_id[0])] = to_matrix(matrix);
                }
            }
        }
    }
    std::unordered_map<std::int64_t, u16> joint_of;
    const auto joint_index = [&](std::int64_t bone, const Matrix& bind) -> u16
    {
        if (const auto known = joint_of.find(bone); known != joint_of.end())
        {
            return known->second;
        }
        GltfJoint joint;
        joint.name = document.objects.at(bone).name;
        joint.bind = {float(bind[12]), float(bind[13]), float(bind[14])};
        scene.joints.push_back(joint);
        joint_of.emplace(bone, u16(scene.joints.size() - 1));
        return u16(scene.joints.size() - 1);
    };
    // Images by texture object.
    std::unordered_map<std::int64_t, int> image_of;
    const auto image_index = [&](std::int64_t texture) -> int
    {
        if (const auto known = image_of.find(texture); known != image_of.end())
        {
            return known->second;
        }
        int index = -1;
        std::vector<u8> png;
        std::string source;
        for (const std::int64_t video : document.children(texture, "Video"))
        {
            const FbxNode* content = document.node(video).find("Content");
            if (content != nullptr && !content->properties.empty() && !content->properties[0].text.empty())
            {
                png.assign(content->properties[0].text.begin(), content->properties[0].text.end());
                source = "embedded";
            }
        }
        const FbxNode& node = document.node(texture);
        if (png.empty())
        {
            const std::filesystem::path relative = text(&node, "RelativeFilename");
            const std::filesystem::path absolute = text(&node, "FileName");
            const std::filesystem::path folder = path.parent_path();
            for (const std::filesystem::path& candidate : {folder / relative, absolute, folder / absolute.filename(),
                folder / (path.stem().string() + ".fbm") / absolute.filename()})
            {
                std::error_code error;
                if (!candidate.empty() && std::filesystem::is_regular_file(candidate, error))
                {
                    png = read_file(candidate);
                    source = candidate.string();
                    break;
                }
            }
        }
        static constexpr u8 kPng[8]{137, 80, 78, 71, 13, 10, 26, 10};
        if (png.size() >= 8 && std::memcmp(png.data(), kPng, 8) == 0)
        {
            scene.images.push_back(decode_png(png));
            scene.image_names.push_back(source);
            index = int(scene.images.size() - 1);
        }
        else
        {
            scene.notes.push_back("texture '" + document.objects.at(texture).name + "' is not a PNG the importer can read ("
                + (png.empty() ? "file not found" : source) + "); its material keeps its texture");
        }
        image_of.emplace(texture, index);
        return index;
    };

    for (const auto& [model_id, model] : document.objects)
    {
        if (model.node->name != "Model" || model.kind != "Mesh")
        {
            continue;
        }
        const auto geometries = document.children(model_id, "Geometry");
        if (geometries.empty())
        {
            continue;
        }
        const std::int64_t geometry_id = geometries.front();
        const FbxNode& geometry = document.node(geometry_id);
        const auto control = numbers(&geometry, "Vertices");
        const auto polygons = numbers(&geometry, "PolygonVertexIndex");
        const size_t controls = control.size() / 3;
        // The skin: per control point, its bones and weights.
        std::vector<std::vector<std::pair<float, u16>>> influences(controls);
        Matrix mesh_global = pose.contains(model_id) ? pose.at(model_id) : document.global(model_id);
        bool skinned = false;
        for (const std::int64_t skin : document.children(geometry_id, "Deformer"))
        {
            if (document.objects.at(skin).kind != "Skin")
            {
                continue;
            }
            for (const std::int64_t cluster : document.children(skin, "Deformer"))
            {
                const auto bones = document.children(cluster, "Model");
                if (bones.empty())
                {
                    continue;
                }
                const FbxNode& node = document.node(cluster);
                const auto link = numbers(&node, "TransformLink");
                const auto transform = numbers(&node, "Transform");
                const Matrix bind = link.size() == 16 ? to_matrix(link)
                    : pose.contains(bones.front()) ? pose.at(bones.front()) : document.global(bones.front());
                if (link.size() == 16 && transform.size() == 16)
                {
                    // TransformLink * Transform is the mesh's global transform when it was bound.
                    mesh_global = multiply(to_matrix(link), to_matrix(transform));
                }
                const u16 joint = joint_index(bones.front(), bind);
                const auto indexes = numbers(&node, "Indexes");
                const auto weights = numbers(&node, "Weights");
                for (size_t item = 0; item < indexes.size() && item < weights.size(); ++item)
                {
                    const size_t point = size_t(indexes[item]);
                    if (point < controls && weights[item] > 0.0)
                    {
                        influences[point].emplace_back(float(weights[item]), joint);
                    }
                }
                skinned = true;
            }
        }
        if (!skinned)
        {
            throw Error("mesh '" + model.name + "' has no Skin deformer; skin it to the exported bones");
        }
        // Shapes: per BlendShape channel, its full-weight shape's deltas by control point.
        std::vector<std::string> morph_names;
        std::vector<std::vector<Vec3>> morph_deltas;
        for (const std::int64_t blend : document.children(geometry_id, "Deformer"))
        {
            if (document.objects.at(blend).kind != "BlendShape")
            {
                continue;
            }
            for (const std::int64_t channel : document.children(blend, "Deformer"))
            {
                const auto shapes = document.children(channel, "Geometry");
                if (shapes.empty())
                {
                    continue;
                }
                const FbxNode& shape = document.node(shapes.back());
                const auto indexes = numbers(&shape, "Indexes");
                const auto deltas = numbers(&shape, "Vertices");
                std::vector<Vec3> per_point(controls);
                for (size_t item = 0; item < indexes.size() && item * 3 + 2 < deltas.size(); ++item)
                {
                    const size_t point = size_t(indexes[item]);
                    if (point < controls)
                    {
                        per_point[point] = direction(mesh_global, {float(deltas[item * 3]), float(deltas[item * 3 + 1]),
                            float(deltas[item * 3 + 2])});
                    }
                }
                morph_names.push_back(document.objects.at(channel).name);
                morph_deltas.push_back(std::move(per_point));
            }
        }
        // Materials in connection order, which the material layer indexes.
        std::vector<std::int64_t> materials = document.children(model_id, "Material");
        const Layer normals(&geometry, "LayerElementNormal", "Normals", "NormalsIndex", 3);
        const Layer uvs(&geometry, "LayerElementUV", "UV", "UVIndex", 2);
        const Layer colours(&geometry, "LayerElementColor", "Colors", "ColorIndex", 4);
        const Layer material_layer(&geometry, "LayerElementMaterial", "Materials", "", 1);
        std::map<int, GltfPrimitive> by_material;
        std::map<int, std::map<std::array<u32, 10>, u32>> lookup;
        size_t corner = 0;
        size_t polygon = 0;
        std::vector<std::pair<size_t, size_t>> current; // (control point, polygon vertex) of the polygon being read.
        const auto vertex_of = [&](GltfPrimitive& out, std::map<std::array<u32, 10>, u32>& keys, size_t point,
            size_t pv) -> u32
        {
            GltfVertex v;
            v.position = point3(mesh_global, point, control);
            if (normals.present())
            {
                const size_t at = normals.at(pv, point, polygon);
                v.normal = direction(mesh_global, {float(normals.values[at]), float(normals.values[at + 1]),
                    float(normals.values[at + 2])});
                const float length = std::sqrt(v.normal.x * v.normal.x + v.normal.y * v.normal.y + v.normal.z * v.normal.z);
                if (length > 0.f)
                {
                    v.normal = {v.normal.x / length, v.normal.y / length, v.normal.z / length};
                }
            }
            if (uvs.present())
            {
                const size_t at = uvs.at(pv, point, polygon);
                v.uv = {float(uvs.values[at]), float(1.0 - uvs.values[at + 1])};
            }
            if (colours.present())
            {
                const size_t at = colours.at(pv, point, polygon);
                const auto channel = [&](size_t offset)
                {
                    return u32(std::lround(std::clamp(colours.values[at + offset], 0.0, 1.0) * 255.0));
                };
                v.colour = channel(2) | channel(1) << 8 | channel(0) << 16 | channel(3) << 24;
            }
            auto list = influences[point];
            std::sort(list.begin(), list.end(), [](const auto& a, const auto& b)
            {
                return a.first > b.first;
            });
            if (list.empty())
            {
                throw Error("a vertex of mesh '" + model.name + "' has no skin weight");
            }
            for (size_t slot = 0; slot < 4; ++slot)
            {
                v.joints[slot] = slot < list.size() ? list[slot].second : list[0].second;
                v.weights[slot] = slot < list.size() ? list[slot].first : 0.f;
            }
            std::array<u32, 10> key{u32(point)};
            std::memcpy(&key[1], &v.normal, 12);
            std::memcpy(&key[4], &v.uv, 8);
            key[6] = v.colour;
            if (const auto known = keys.find(key); known != keys.end())
            {
                return known->second;
            }
            const u32 index = u32(out.vertices.size());
            out.vertices.push_back(v);
            for (size_t morph = 0; morph < morph_deltas.size(); ++morph)
            {
                out.morph_positions[morph].push_back(morph_deltas[morph][point]);
            }
            keys.emplace(key, index);
            return index;
        };
        for (const double raw : polygons)
        {
            const std::int64_t value = std::int64_t(raw);
            const bool last = value < 0;
            const size_t point = size_t(last ? -value - 1 : value);
            if (point >= controls)
            {
                throw Error("mesh '" + model.name + "' indexes a control point it does not have");
            }
            current.emplace_back(point, corner);
            ++corner;
            if (!last)
            {
                continue;
            }
            int material = 0;
            if (material_layer.present())
            {
                material = int(material_layer.values[material_layer.at(corner - 1, point, polygon)]);
            }
            GltfPrimitive& out = by_material[material];
            if (out.vertices.empty() && out.mesh.empty())
            {
                out.mesh = model.name;
                out.has_normals = normals.present();
                out.has_colours = colours.present();
                out.morph_names = morph_names;
                out.morph_positions.resize(morph_names.size());
                out.morph_normals.resize(morph_names.size());
                if (material >= 0 && size_t(material) < materials.size())
                {
                    const std::int64_t material_id = materials[size_t(material)];
                    out.material = document.objects.at(material_id).name;
                    std::int64_t texture = 0;
                    for (const Link& link : document.links)
                    {
                        if (link.parent == material_id && document.objects.contains(link.child)
                            && document.objects.at(link.child).node->name == "Texture"
                            && (texture == 0 || link.property == "DiffuseColor"))
                        {
                            texture = link.child;
                        }
                    }
                    if (texture != 0)
                    {
                        out.image = image_index(texture);
                    }
                }
            }
            auto& keys = lookup[material];
            // A fan over the polygon.
            for (size_t index = 1; index + 1 < current.size(); ++index)
            {
                for (const size_t c : {size_t(0), index, index + 1})
                {
                    out.indices.push_back(vertex_of(out, keys, current[c].first, current[c].second));
                }
            }
            current.clear();
            ++polygon;
        }
        for (auto& [material, primitive] : by_material)
        {
            scene.primitives.push_back(std::move(primitive));
        }
    }
    if (scene.primitives.empty())
    {
        throw Error(path.string() + " has no skinned mesh");
    }
    return scene;
}
}
#endif
