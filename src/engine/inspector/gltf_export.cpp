#if defined(GBTG_INSPECTOR_APP) && !defined(GBTG_RETAIL_GAME_ONLY)
#include "gltf_export.h"

#include <cstdio>
#include <sstream>

namespace gbtg
{
namespace
{
std::string quoted(const std::string& text)
{
    std::string out = "\"";
    for (const char c : text)
    {
        if (c == '"' || c == '\\')
        {
            out += '\\';
            out += c;
        }
        else if (static_cast<unsigned char>(c) < 0x20)
        {
            char escape[8];
            std::snprintf(escape, sizeof(escape), "\\u%04x", unsigned(static_cast<unsigned char>(c)));
            out += escape;
        }
        else
        {
            out += c;
        }
    }
    return out + "\"";
}

std::string number(float value)
{
    char text[32];
    std::snprintf(text, sizeof(text), "%.9g", double(value));
    return text;
}

// Binary data and the accessors / buffer views over it.
struct GltfBuffer
{
    std::vector<u8> bytes;
    std::vector<std::string> views;
    std::vector<std::string> accessors;

    size_t view(std::span<const u8> data, int target)
    {
        while (bytes.size() % 4 != 0)
        {
            bytes.push_back(0);
        }
        const size_t offset = bytes.size();
        bytes.insert(bytes.end(), data.begin(), data.end());
        std::string text = "{\"buffer\":0,\"byteOffset\":" + std::to_string(offset) + ",\"byteLength\":"
            + std::to_string(data.size());
        if (target != 0)
        {
            text += ",\"target\":" + std::to_string(target);
        }
        views.push_back(text + "}");
        return views.size() - 1;
    }

    template <class T>
    size_t accessor(const std::vector<T>& values, int component, int components, const char* type, int target,
        bool bounds = false, bool normalized = false)
    {
        const size_t index = view({reinterpret_cast<const u8*>(values.data()), values.size() * sizeof(T)}, target);
        std::string text = "{\"bufferView\":" + std::to_string(index) + ",\"componentType\":"
            + std::to_string(component) + ",\"count\":" + std::to_string(values.size() * sizeof(T)
            / (size_t(components) * (component == 5126 || component == 5125 ? 4 : component == 5123 ? 2 : 1)))
            + ",\"type\":\"" + type + "\"";
        if (normalized)
        {
            text += ",\"normalized\":true";
        }
        if (bounds)
        {
            // POSITION accessors must carry their bounds.
            const float* data = reinterpret_cast<const float*>(values.data());
            const size_t count = values.size() * sizeof(T) / (sizeof(float) * size_t(components));
            std::vector<float> low(size_t(components), 0.f);
            std::vector<float> high(size_t(components), 0.f);
            for (size_t item = 0; item < count; ++item)
            {
                for (int axis = 0; axis < components; ++axis)
                {
                    const float value = data[item * size_t(components) + size_t(axis)];
                    if (item == 0 || value < low[size_t(axis)])
                    {
                        low[size_t(axis)] = value;
                    }
                    if (item == 0 || value > high[size_t(axis)])
                    {
                        high[size_t(axis)] = value;
                    }
                }
            }
            text += ",\"min\":[";
            for (int axis = 0; axis < components; ++axis)
            {
                text += (axis ? "," : "") + number(low[size_t(axis)]);
            }
            text += "],\"max\":[";
            for (int axis = 0; axis < components; ++axis)
            {
                text += (axis ? "," : "") + number(high[size_t(axis)]);
            }
            text += "]";
        }
        accessors.push_back(text + "}");
        return accessors.size() - 1;
    }
};

std::string join(const std::vector<std::string>& items)
{
    std::string out;
    for (size_t index = 0; index < items.size(); ++index)
    {
        out += (index ? "," : "") + items[index];
    }
    return out;
}
}

std::vector<u8> write_gltf(const DecodedModel& model)
{
    GltfBuffer buffer;
    std::vector<std::string> nodes;
    std::vector<std::string> meshes;
    std::vector<std::string> materials;
    std::vector<std::string> images;
    std::vector<std::string> textures;
    // Joints: nodes 0..n-1, translated from their parent's bind point, unrotated.
    std::vector<std::vector<size_t>> children(model.joints.size());
    std::vector<size_t> root_joints;
    for (size_t joint = 0; joint < model.joints.size(); ++joint)
    {
        const int parent = model.joints[joint].parent;
        if (parent >= 0 && size_t(parent) < model.joints.size())
        {
            children[size_t(parent)].push_back(joint);
        }
        else
        {
            root_joints.push_back(joint);
        }
    }
    for (size_t joint = 0; joint < model.joints.size(); ++joint)
    {
        const ModelJoint& record = model.joints[joint];
        Vec3 local = record.world;
        if (record.parent >= 0 && size_t(record.parent) < model.joints.size())
        {
            const Vec3& parent = model.joints[size_t(record.parent)].world;
            local = {local.x - parent.x, local.y - parent.y, local.z - parent.z};
        }
        std::string text = "{\"name\":" + quoted(record.name) + ",\"translation\":[" + number(local.x) + ","
            + number(local.y) + "," + number(local.z) + "]";
        if (!children[joint].empty())
        {
            std::vector<std::string> list;
            for (const size_t child : children[joint])
            {
                list.push_back(std::to_string(child));
            }
            text += ",\"children\":[" + join(list) + "]";
        }
        nodes.push_back(text + "}");
    }
    std::vector<float> inverse_bind;
    for (const ModelJoint& joint : model.joints)
    {
        const float matrix[16]{1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, -joint.world.x, -joint.world.y,
            -joint.world.z, 1.f};
        inverse_bind.insert(inverse_bind.end(), std::begin(matrix), std::end(matrix));
    }
    const size_t inverse_bind_accessor = buffer.accessor(inverse_bind, 5126, 16, "MAT4", 0);
    // Images: each texture a material samples as its colour.
    std::map<i32, size_t> texture_of;
    for (const ModelMaterialInfo& material : model.materials)
    {
        const i32 index = material.colour_texture;
        if (index < 0 || size_t(index) >= model.textures.size() || texture_of.contains(index)
            || model.textures[size_t(index)].image.bgra.empty())
        {
            continue;
        }
        const auto png = encode_png(model.textures[size_t(index)].image);
        const size_t view = buffer.view(png, 0);
        images.push_back("{\"name\":\"texture" + std::to_string(index) + "\",\"bufferView\":" + std::to_string(view)
            + ",\"mimeType\":\"image/png\"}");
        textures.push_back("{\"sampler\":0,\"source\":" + std::to_string(images.size() - 1) + "}");
        texture_of.emplace(index, textures.size() - 1);
    }
    std::map<u32, size_t> material_of;
    for (const ModelMaterialInfo& material : model.materials)
    {
        std::string text = "{\"name\":" + quoted(material.name) + ",\"pbrMetallicRoughness\":{";
        if (const auto texture = texture_of.find(material.colour_texture); texture != texture_of.end())
        {
            text += "\"baseColorTexture\":{\"index\":" + std::to_string(texture->second) + "},";
        }
        text += "\"metallicFactor\":0,\"roughnessFactor\":1},\"extras\":{\"gbtg_node\":" + std::to_string(material.node)
            + ",\"gbtg_colour_texture\":" + std::to_string(material.colour_texture) + ",\"gbtg_celmap_texture\":"
            + std::to_string(material.celmap_texture) + ",\"gbtg_lightmap_texture\":"
            + std::to_string(material.lightmap_texture) + "}}";
        material_of.emplace(material.node, materials.size());
        materials.push_back(text);
    }
    // Two meshes: the body, and the blend-shape branch (head) with its morph targets.
    std::vector<size_t> mesh_nodes;
    for (const bool head : {false, true})
    {
        std::vector<std::string> primitives;
        for (const ModelPrimitive& primitive : model.primitives)
        {
            if (primitive.head != head || primitive.indices.empty())
            {
                continue;
            }
            std::vector<float> positions, normals, uvs, weights;
            std::vector<u16> joints;
            std::vector<u8> colours;
            for (const ModelVertex& vertex : primitive.vertices)
            {
                positions.insert(positions.end(), {vertex.position.x, vertex.position.y, vertex.position.z});
                normals.insert(normals.end(), {vertex.normal.x, vertex.normal.y, vertex.normal.z});
                uvs.insert(uvs.end(), {vertex.uv.u, vertex.uv.v});
                joints.insert(joints.end(), {vertex.joints[0], vertex.joints[1], 0, 0});
                // D3DCOLOR is B, G, R, A in memory.
                colours.insert(colours.end(), {u8(vertex.colour >> 16), u8(vertex.colour >> 8), u8(vertex.colour),
                    u8(vertex.colour >> 24)});
                const float total = vertex.weights[0] + vertex.weights[1];
                weights.insert(weights.end(), {vertex.weights[0] / total, vertex.weights[1] / total, 0.f, 0.f});
            }
            std::string text = "{\"attributes\":{\"POSITION\":"
                + std::to_string(buffer.accessor(positions, 5126, 3, "VEC3", 34962, true)) + ",\"NORMAL\":"
                + std::to_string(buffer.accessor(normals, 5126, 3, "VEC3", 34962)) + ",\"TEXCOORD_0\":"
                + std::to_string(buffer.accessor(uvs, 5126, 2, "VEC2", 34962)) + ",\"JOINTS_0\":"
                + std::to_string(buffer.accessor(joints, 5123, 4, "VEC4", 34962)) + ",\"WEIGHTS_0\":"
                + std::to_string(buffer.accessor(weights, 5126, 4, "VEC4", 34962)) + ",\"COLOR_0\":"
                + std::to_string(buffer.accessor(colours, 5121, 4, "VEC4", 34962, false, true)) + "},\"indices\":"
                + std::to_string(buffer.accessor(primitive.indices, 5125, 1, "SCALAR", 34963)) + ",\"material\":"
                + std::to_string(material_of.at(primitive.material_node));
            if (!primitive.morphs.empty())
            {
                std::vector<std::string> targets;
                for (const auto& morph : primitive.morphs)
                {
                    std::vector<float> deltas;
                    for (const Vec3& delta : morph)
                    {
                        deltas.insert(deltas.end(), {delta.x, delta.y, delta.z});
                    }
                    targets.push_back("{\"POSITION\":" + std::to_string(buffer.accessor(deltas, 5126, 3, "VEC3", 34962,
                        true)) + "}");
                }
                text += ",\"targets\":[" + join(targets) + "]";
            }
            primitives.push_back(text + "}");
        }
        if (primitives.empty())
        {
            continue;
        }
        std::string mesh = "{\"name\":\"" + std::string(head ? "head" : "body") + "\",\"primitives\":["
            + join(primitives) + "]";
        if (head && !model.morph_names.empty())
        {
            std::vector<std::string> names;
            for (const auto& name : model.morph_names)
            {
                names.push_back(quoted(name));
            }
            mesh += ",\"extras\":{\"targetNames\":[" + join(names) + "]}";
        }
        meshes.push_back(mesh + "}");
        nodes.push_back("{\"name\":\"" + std::string(head ? "head" : "body") + "\",\"mesh\":"
            + std::to_string(meshes.size() - 1) + ",\"skin\":0}");
        mesh_nodes.push_back(nodes.size() - 1);
    }
    // The armature root holds the root joints and the meshes.
    std::vector<std::string> root_children;
    for (const size_t joint : root_joints)
    {
        root_children.push_back(std::to_string(joint));
    }
    for (const size_t node : mesh_nodes)
    {
        root_children.push_back(std::to_string(node));
    }
    nodes.push_back("{\"name\":" + quoted(model.aid) + ",\"children\":[" + join(root_children) + "]}");
    const size_t root = nodes.size() - 1;
    std::vector<std::string> joint_list;
    for (size_t joint = 0; joint < model.joints.size(); ++joint)
    {
        joint_list.push_back(std::to_string(joint));
    }
    std::ostringstream json;
    json << "{\"asset\":{\"version\":\"2.0\",\"generator\":\"gbtg-inspector model export\",\"extras\":{\"gbtg_model\":"
         << quoted(model.aid) << ",\"gbtg_winding_flipped\":" << (model.winding_flipped ? "true" : "false") << "}},"
         << "\"scene\":0,\"scenes\":[{\"nodes\":[" << root << "]}],"
         << "\"nodes\":[" << join(nodes) << "],"
         << "\"meshes\":[" << join(meshes) << "],"
         << "\"skins\":[{\"name\":\"skeleton\",\"inverseBindMatrices\":" << inverse_bind_accessor;
    if (!root_joints.empty())
    {
        json << ",\"skeleton\":" << root_joints.front();
    }
    json << ",\"joints\":[" << join(joint_list) << "]}],"
         << "\"materials\":[" << join(materials) << "],";
    if (!images.empty())
    {
        json << "\"images\":[" << join(images) << "],\"textures\":[" << join(textures) << "],"
             << "\"samplers\":[{\"magFilter\":9729,\"minFilter\":9729,\"wrapS\":10497,\"wrapT\":10497}],";
    }
    json << "\"accessors\":[" << join(buffer.accessors) << "],"
         << "\"bufferViews\":[" << join(buffer.views) << "],"
         << "\"buffers\":[{\"byteLength\":" << buffer.bytes.size() << "}]}";
    std::string text = json.str();
    while (text.size() % 4 != 0)
    {
        text += ' ';
    }
    while (buffer.bytes.size() % 4 != 0)
    {
        buffer.bytes.push_back(0);
    }
    std::vector<u8> glb;
    const auto append = [&glb](u32 value)
    {
        const u8* bytes = reinterpret_cast<const u8*>(&value);
        glb.insert(glb.end(), bytes, bytes + 4);
    };
    append(0x46546C67); // "glTF"
    append(2);
    append(u32(12 + 8 + text.size() + 8 + buffer.bytes.size()));
    append(u32(text.size()));
    append(0x4E4F534A); // "JSON"
    glb.insert(glb.end(), text.begin(), text.end());
    append(u32(buffer.bytes.size()));
    append(0x004E4942); // "BIN"
    glb.insert(glb.end(), buffer.bytes.begin(), buffer.bytes.end());
    return glb;
}
}
#endif
