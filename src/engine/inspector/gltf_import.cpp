#if defined(GBTG_INSPECTOR_APP) && !defined(GBTG_RETAIL_GAME_ONLY)
#include "gltf_import.h"
#include "json_value.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace gbtg
{
namespace
{
std::string without_suffix(const std::string& name)
{
    // Blender renames a duplicate material "name.001".
    const size_t dot = name.rfind('.');
    if (dot != std::string::npos && dot + 1 < name.size()
        && std::all_of(name.begin() + std::ptrdiff_t(dot + 1), name.end(), [](char c)
        {
            return c >= '0' && c <= '9';
        }))
    {
        return name.substr(0, dot);
    }
    return name;
}

bool invert4(const double m[16], double out[16])
{
    double inv[16];
    inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11]
        - m[13] * m[7] * m[10];
    inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11]
        + m[12] * m[7] * m[10];
    inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] + m[12] * m[5] * m[11]
        - m[12] * m[7] * m[9];
    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10]
        + m[12] * m[6] * m[9];
    inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11]
        + m[13] * m[3] * m[10];
    inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] + m[12] * m[2] * m[11]
        - m[12] * m[3] * m[10];
    inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] - m[12] * m[1] * m[11]
        + m[12] * m[3] * m[9];
    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] + m[12] * m[1] * m[10]
        - m[12] * m[2] * m[9];
    inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[7]
        - m[13] * m[3] * m[6];
    inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] - m[12] * m[2] * m[7]
        + m[12] * m[3] * m[6];
    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] + m[12] * m[1] * m[7]
        - m[12] * m[3] * m[5];
    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] - m[12] * m[1] * m[6]
        + m[12] * m[2] * m[5];
    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[9] * m[2] * m[7]
        + m[9] * m[3] * m[6];
    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[8] * m[2] * m[7]
        - m[8] * m[3] * m[6];
    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] - m[8] * m[1] * m[7]
        + m[8] * m[3] * m[5];
    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6]
        - m[8] * m[2] * m[5];
    const double det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
    if (std::abs(det) < 1e-12)
    {
        return false;
    }
    for (int i = 0; i < 16; ++i)
    {
        out[i] = inv[i] / det;
    }
    return true;
}

struct Glb
{
    JsonValue json;
    std::vector<u8> bin;

    // An accessor's components as doubles, `components` per element (normalized integers in 0..1 / -1..1).
    std::vector<double> read(size_t accessor_index, int components) const
    {
        const JsonValue& accessor = json["accessors"][accessor_index];
        if (accessor.is_null())
        {
            throw Error("glTF accessor " + std::to_string(accessor_index) + " is missing");
        }
        const size_t count = size_t(accessor["count"].integer());
        const int type = int(accessor["componentType"].integer());
        const bool normalized = accessor["normalized"].boolean();
        std::vector<double> out(count * size_t(components), 0.0);
        if (accessor.has("bufferView"))
        {
            read_view(size_t(accessor["bufferView"].integer()), size_t(accessor["byteOffset"].integer()), type,
                normalized, components, count, out.data());
        }
        if (accessor.has("sparse"))
        {
            // Replaced elements: an index list and their values (Blender writes morph targets this way).
            const JsonValue& sparse = accessor["sparse"];
            const size_t replaced = size_t(sparse["count"].integer());
            std::vector<double> indices(replaced);
            read_view(size_t(sparse["indices"]["bufferView"].integer()), size_t(sparse["indices"]["byteOffset"].integer()),
                int(sparse["indices"]["componentType"].integer()), false, 1, replaced, indices.data());
            std::vector<double> values(replaced * size_t(components));
            read_view(size_t(sparse["values"]["bufferView"].integer()), size_t(sparse["values"]["byteOffset"].integer()),
                type, normalized, components, replaced, values.data());
            for (size_t item = 0; item < replaced; ++item)
            {
                const size_t index = size_t(indices[item]);
                if (index >= count)
                {
                    throw Error("a sparse glTF accessor replaces an element past its count");
                }
                for (int component = 0; component < components; ++component)
                {
                    out[index * size_t(components) + size_t(component)] = values[item * size_t(components) + size_t(component)];
                }
            }
        }
        return out;
    }

    void read_view(size_t view_index, size_t offset, int type, bool normalized, int components, size_t count,
        double* out) const
    {
        const size_t size = type == 5126 || type == 5125 ? 4 : type == 5123 || type == 5122 ? 2 : 1;
        const JsonValue& view = json["bufferViews"][view_index];
        if (view["buffer"].integer() != 0)
        {
            throw Error("only the .glb binary buffer is supported");
        }
        const size_t stride = view.has("byteStride") ? size_t(view["byteStride"].integer()) : size * size_t(components);
        const size_t base = size_t(view["byteOffset"].integer()) + offset;
        for (size_t item = 0; item < count; ++item)
        {
            for (int component = 0; component < components; ++component)
            {
                const size_t at = base + item * stride + size_t(component) * size;
                if (at + size > bin.size())
                {
                    throw Error("glTF accessor reads past its buffer");
                }
                const u8* p = bin.data() + at;
                double value = 0.0;
                switch (type)
                {
                case 5126:
                {
                    float f;
                    std::memcpy(&f, p, 4);
                    value = f;
                    break;
                }
                case 5125:
                {
                    u32 v;
                    std::memcpy(&v, p, 4);
                    value = v;
                    break;
                }
                case 5123:
                {
                    u16 v;
                    std::memcpy(&v, p, 2);
                    value = normalized ? v / 65535.0 : v;
                    break;
                }
                case 5122:
                {
                    std::int16_t v;
                    std::memcpy(&v, p, 2);
                    value = normalized ? std::max(v / 32767.0, -1.0) : v;
                    break;
                }
                case 5121:
                    value = normalized ? *p / 255.0 : *p;
                    break;
                case 5120:
                {
                    const std::int8_t v = std::int8_t(*p);
                    value = normalized ? std::max(v / 127.0, -1.0) : v;
                    break;
                }
                default:
                    throw Error("glTF accessor has an unknown component type");
                }
                out[item * size_t(components) + size_t(component)] = value;
            }
        }
    }
};

int components_of(const JsonValue& accessor)
{
    const std::string& type = accessor["type"].string();
    return type == "SCALAR" ? 1 : type == "VEC2" ? 2 : type == "VEC3" ? 3 : type == "VEC4" ? 4 : type == "MAT4" ? 16 : 0;
}
}

GltfScene read_glb(const std::filesystem::path& path)
{
    const std::vector<u8> file = read_file(path);
    if (file.size() < 20 || read_u32(file, 0) != 0x46546C67 || read_u32(file, 4) != 2)
    {
        throw Error(path.string() + " is not a glTF 2.0 binary (.glb)");
    }
    Glb glb;
    size_t at = 12;
    while (at + 8 <= file.size())
    {
        const u32 length = read_u32(file, at);
        const u32 type = read_u32(file, at + 4);
        if (at + 8 + length > file.size())
        {
            throw Error("truncated .glb chunk");
        }
        if (type == 0x4E4F534A)
        {
            glb.json = JsonValue::parse(std::string_view(reinterpret_cast<const char*>(file.data() + at + 8), length));
        }
        else if (type == 0x004E4942)
        {
            glb.bin.assign(file.begin() + std::ptrdiff_t(at + 8), file.begin() + std::ptrdiff_t(at + 8 + length));
        }
        at += 8 + length;
    }
    const JsonValue& json = glb.json;
    GltfScene scene;
    // Images.
    for (size_t index = 0; index < json["images"].size(); ++index)
    {
        const JsonValue& image = json["images"][index];
        scene.image_names.push_back(image["name"].string());
        if (!image.has("bufferView"))
        {
            throw Error("glTF image " + std::to_string(index) + " is not embedded in the .glb");
        }
        if (image.has("mimeType") && image["mimeType"].string() != "image/png")
        {
            throw Error("glTF image " + std::to_string(index) + " is " + image["mimeType"].string() + "; export PNG");
        }
        const JsonValue& view = json["bufferViews"][size_t(image["bufferView"].integer())];
        const size_t offset = size_t(view["byteOffset"].integer());
        const size_t length = size_t(view["byteLength"].integer());
        if (offset + length > glb.bin.size())
        {
            throw Error("glTF image reads past its buffer");
        }
        scene.images.push_back(decode_png({glb.bin.data() + offset, length}));
    }
    // The skin: every skinned mesh must use the same one.
    int skin_index = -1;
    for (size_t node = 0; node < json["nodes"].size(); ++node)
    {
        const JsonValue& record = json["nodes"][node];
        if (!record.has("mesh"))
        {
            continue;
        }
        if (!record.has("skin"))
        {
            throw Error("mesh node '" + record["name"].string() + "' is not skinned to the skeleton");
        }
        const int skin = int(record["skin"].integer());
        if (skin_index >= 0 && skin != skin_index)
        {
            throw Error("all meshes must use one skin");
        }
        skin_index = skin;
    }
    if (skin_index < 0)
    {
        throw Error("the .glb has no skinned mesh");
    }
    const JsonValue& skin = json["skins"][size_t(skin_index)];
    std::vector<double> inverse_bind;
    if (skin.has("inverseBindMatrices"))
    {
        inverse_bind = glb.read(size_t(skin["inverseBindMatrices"].integer()), 16);
    }
    for (size_t joint = 0; joint < skin["joints"].size(); ++joint)
    {
        GltfJoint out;
        out.name = json["nodes"][size_t(skin["joints"][joint].integer())]["name"].string();
        if (inverse_bind.size() >= (joint + 1) * 16)
        {
            double inverse[16];
            if (!invert4(inverse_bind.data() + joint * 16, inverse))
            {
                throw Error("joint '" + out.name + "' has a singular inverse bind matrix");
            }
            out.bind = {float(inverse[12]), float(inverse[13]), float(inverse[14])};
        }
        scene.joints.push_back(out);
    }
    // Primitives.
    for (size_t node = 0; node < json["nodes"].size(); ++node)
    {
        const JsonValue& record = json["nodes"][node];
        if (!record.has("mesh"))
        {
            continue;
        }
        const JsonValue& mesh = json["meshes"][size_t(record["mesh"].integer())];
        std::vector<std::string> target_names;
        for (const JsonValue& name : mesh["extras"]["targetNames"].items())
        {
            target_names.push_back(name.string());
        }
        for (const JsonValue& primitive : mesh["primitives"].items())
        {
            if (primitive.has("mode") && primitive["mode"].integer() != 4)
            {
                throw Error("mesh '" + mesh["name"].string() + "' has a primitive that is not triangles");
            }
            const JsonValue& attributes = primitive["attributes"];
            if (!attributes.has("POSITION") || !attributes.has("JOINTS_0") || !attributes.has("WEIGHTS_0"))
            {
                throw Error("mesh '" + mesh["name"].string() + "' needs POSITION, JOINTS_0 and WEIGHTS_0");
            }
            GltfPrimitive out;
            out.mesh = mesh["name"].string().empty() ? record["name"].string() : mesh["name"].string();
            if (primitive.has("material"))
            {
                const JsonValue& material = json["materials"][size_t(primitive["material"].integer())];
                out.material = without_suffix(material["name"].string());
                const JsonValue& base = material["pbrMetallicRoughness"]["baseColorTexture"];
                if (base.has("index"))
                {
                    out.image = int(json["textures"][size_t(base["index"].integer())]["source"].integer(-1));
                }
            }
            const auto positions = glb.read(size_t(attributes["POSITION"].integer()), 3);
            const size_t count = positions.size() / 3;
            out.vertices.resize(count);
            const auto joints = glb.read(size_t(attributes["JOINTS_0"].integer()), 4);
            const auto weights = glb.read(size_t(attributes["WEIGHTS_0"].integer()), 4);
            std::vector<double> normals, uvs, colours;
            int colour_components = 0;
            if (attributes.has("NORMAL"))
            {
                normals = glb.read(size_t(attributes["NORMAL"].integer()), 3);
                out.has_normals = true;
            }
            if (attributes.has("TEXCOORD_0"))
            {
                uvs = glb.read(size_t(attributes["TEXCOORD_0"].integer()), 2);
            }
            if (attributes.has("COLOR_0"))
            {
                const JsonValue& accessor = json["accessors"][size_t(attributes["COLOR_0"].integer())];
                colour_components = components_of(accessor);
                colours = glb.read(size_t(attributes["COLOR_0"].integer()), colour_components);
                out.has_colours = true;
            }
            for (size_t vertex = 0; vertex < count; ++vertex)
            {
                GltfVertex& v = out.vertices[vertex];
                v.position = {float(positions[vertex * 3]), float(positions[vertex * 3 + 1]),
                    float(positions[vertex * 3 + 2])};
                if (!normals.empty())
                {
                    v.normal = {float(normals[vertex * 3]), float(normals[vertex * 3 + 1]), float(normals[vertex * 3 + 2])};
                }
                if (!uvs.empty())
                {
                    v.uv = {float(uvs[vertex * 2]), float(uvs[vertex * 2 + 1])};
                }
                if (!colours.empty())
                {
                    const auto channel = [&](int component, double fallback)
                    {
                        const double value = component < colour_components
                            ? colours[vertex * size_t(colour_components) + size_t(component)] : fallback;
                        return u32(std::lround(std::clamp(value, 0.0, 1.0) * 255.0));
                    };
                    v.colour = channel(2, 1.0) | channel(1, 1.0) << 8 | channel(0, 1.0) << 16 | channel(3, 1.0) << 24;
                }
                for (int slot = 0; slot < 4; ++slot)
                {
                    const double joint = joints[vertex * 4 + size_t(slot)];
                    if (joint < 0 || joint >= double(scene.joints.size()))
                    {
                        throw Error("a vertex names a joint outside the skin");
                    }
                    v.joints[size_t(slot)] = u16(joint);
                    v.weights[size_t(slot)] = float(weights[vertex * 4 + size_t(slot)]);
                }
            }
            if (primitive.has("indices"))
            {
                const auto indices = glb.read(size_t(primitive["indices"].integer()), 1);
                for (const double index : indices)
                {
                    if (index < 0 || index >= double(count))
                    {
                        throw Error("a triangle index is outside its vertices");
                    }
                    out.indices.push_back(u32(index));
                }
            }
            else
            {
                for (u32 index = 0; index < count; ++index)
                {
                    out.indices.push_back(index);
                }
            }
            out.indices.resize(out.indices.size() / 3 * 3);
            for (size_t target = 0; target < primitive["targets"].size(); ++target)
            {
                const JsonValue& morph = primitive["targets"][target];
                out.morph_names.push_back(target < target_names.size() ? target_names[target]
                    : "target_" + std::to_string(target));
                std::vector<Vec3> deltas(count);
                if (morph.has("POSITION"))
                {
                    const auto values = glb.read(size_t(morph["POSITION"].integer()), 3);
                    for (size_t vertex = 0; vertex < count; ++vertex)
                    {
                        deltas[vertex] = {float(values[vertex * 3]), float(values[vertex * 3 + 1]),
                            float(values[vertex * 3 + 2])};
                    }
                }
                out.morph_positions.push_back(std::move(deltas));
                std::vector<Vec3> normal_deltas;
                if (morph.has("NORMAL"))
                {
                    const auto values = glb.read(size_t(morph["NORMAL"].integer()), 3);
                    normal_deltas.resize(count);
                    for (size_t vertex = 0; vertex < count; ++vertex)
                    {
                        normal_deltas[vertex] = {float(values[vertex * 3]), float(values[vertex * 3 + 1]),
                            float(values[vertex * 3 + 2])};
                    }
                }
                out.morph_normals.push_back(std::move(normal_deltas));
            }
            scene.primitives.push_back(std::move(out));
        }
    }
    return scene;
}
}
#endif
