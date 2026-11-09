#if defined(GBTG_INSPECTOR_APP) && !defined(GBTG_RETAIL_GAME_ONLY)
#include "fbx_export.h"
#include "fbx_document.h"

#include <algorithm>
#include <map>

namespace gbtg
{
namespace
{
using P = FbxProperty;

std::string object_name(const std::string& name, const char* kind)
{
    return name + std::string("\x00\x01", 2) + kind;
}

// One Properties70 entry.
FbxNode& property(FbxNode& properties70, const char* name, const char* type, const char* label, const char* flags)
{
    return properties70.add("P", {P::string(name), P::string(type), P::string(label), P::string(flags)});
}
void property_int(FbxNode& properties70, const char* name, int value)
{
    property(properties70, name, "int", "Integer", "").properties.push_back(P::i32(value));
}
void property_double(FbxNode& properties70, const char* name, double value)
{
    property(properties70, name, "double", "Number", "").properties.push_back(P::f64(value));
}
void property_vector(FbxNode& properties70, const char* name, const char* type, const char* label, const char* flags,
    double x, double y, double z)
{
    auto& p = property(properties70, name, type, label, flags);
    p.properties.insert(p.properties.end(), {P::f64(x), P::f64(y), P::f64(z)});
}

// What every model ends with: its node attribute (the first one: a mesh's geometry, a bone's skeleton attribute;
// without it the FBX SDK makes an empty node) and inheritance, then the flags the SDK writes.
void model_tail(FbxNode& model, FbxNode& properties70)
{
    property(properties70, "DefaultAttributeIndex", "int", "Integer", "").properties.push_back(P::i32(0));
    property(properties70, "InheritType", "enum", "", "").properties.push_back(P::i32(1));
    model.add("MultiLayer", {P::i32(0)});
    model.add("MultiTake", {P::i32(0)});
    model.add("Shading", {P::boolean(true)});
    model.add("Culling", {P::string("CullingOff")});
}

std::vector<double> translation_matrix(const Vec3& t)
{
    return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, t.x, t.y, t.z, 1};
}

struct Ids
{
    std::int64_t next = 2000000;
    std::int64_t operator()()
    {
        return next++;
    }
};
}

void write_fbx_model(const DecodedModel& model, const std::filesystem::path& out)
{
    Ids id;
    std::vector<FbxNode> top;
    std::vector<FbxNode> objects;
    std::vector<FbxNode> connections;
    const auto connect = [&connections](std::int64_t child, std::int64_t parent)
    {
        connections.push_back(FbxNode{"C", {P::string("OO"), P::i64(child), P::i64(parent)}, {}});
    };
    const auto connect_property = [&connections](std::int64_t child, std::int64_t parent, const char* name)
    {
        connections.push_back(FbxNode{"C", {P::string("OP"), P::i64(child), P::i64(parent), P::string(name)}, {}});
    };
    std::map<std::string, int> counts;

    // Header records.
    {
        FbxNode& header = top.emplace_back(FbxNode{"FBXHeaderExtension", {}, {}});
        header.add("FBXHeaderVersion", {P::i32(1003)});
        header.add("FBXVersion", {P::i32(7400)});
        header.add("EncryptionType", {P::i32(0)});
        FbxNode& stamp = header.add("CreationTimeStamp");
        stamp.add("Version", {P::i32(1000)});
        for (const auto& [name, value] : std::initializer_list<std::pair<const char*, int>>{{"Year", 2026}, {"Month", 1},
            {"Day", 1}, {"Hour", 0}, {"Minute", 0}, {"Second", 0}, {"Millisecond", 0}})
        {
            stamp.add(name, {P::i32(value)});
        }
        header.add("Creator", {P::string("gbtg-inspector model export")});
        top.push_back(FbxNode{"FileId", {P::raw(std::string("\x28\xb3\x2a\xeb\xb6\x24\xcc\xc2\xbf\xc8\xb0\x2a\xa9\x2b\xfc\xf1", 16))}, {}});
        top.push_back(FbxNode{"CreationTime", {P::string("1970-01-01 10:00:00:000")}, {}});
        top.push_back(FbxNode{"Creator", {P::string("gbtg-inspector model export")}, {}});
        FbxNode& settings = top.emplace_back(FbxNode{"GlobalSettings", {}, {}});
        settings.add("Version", {P::i32(1000)});
        FbxNode& p70 = settings.add("Properties70");
        // Y up, Z front, X right (right-handed), one unit to the game's unit.
        property_int(p70, "UpAxis", 1);
        property_int(p70, "UpAxisSign", 1);
        property_int(p70, "FrontAxis", 2);
        property_int(p70, "FrontAxisSign", 1);
        property_int(p70, "CoordAxis", 0);
        property_int(p70, "CoordAxisSign", 1);
        property_int(p70, "OriginalUpAxis", 1);
        property_int(p70, "OriginalUpAxisSign", 1);
        property_double(p70, "UnitScaleFactor", 1.0);
        property_double(p70, "OriginalUnitScaleFactor", 1.0);
        property(p70, "TimeMode", "enum", "", "").properties.push_back(P::i32(11));
        FbxNode& documents = top.emplace_back(FbxNode{"Documents", {}, {}});
        documents.add("Count", {P::i32(1)});
        FbxNode& document = documents.add("Document", {P::i64(id()), P::string("Scene"), P::string("Scene")});
        FbxNode& document_p70 = document.add("Properties70");
        property(document_p70, "SourceObject", "object", "", "");
        property(document_p70, "ActiveAnimStackName", "KString", "", "").properties.push_back(P::string(""));
        document.add("RootNode", {P::i64(0)});
        top.push_back(FbxNode{"References", {}, {}});
    }

    // Joints.
    std::vector<std::int64_t> joint_ids(model.joints.size());
    for (size_t joint = 0; joint < model.joints.size(); ++joint)
    {
        const ModelJoint& record = model.joints[joint];
        joint_ids[joint] = id();
        const std::int64_t attribute = id();
        FbxNode& node = objects.emplace_back(FbxNode{"NodeAttribute", {P::i64(attribute),
            P::string(object_name(record.name, "NodeAttribute")), P::string("LimbNode")}, {}});
        property_double(node.add("Properties70"), "Size", 1.0);
        node.add("TypeFlags", {P::string("Skeleton")});
        ++counts["NodeAttribute"];
        Vec3 local = record.world;
        if (record.parent >= 0 && size_t(record.parent) < model.joints.size())
        {
            const Vec3& parent = model.joints[size_t(record.parent)].world;
            local = {local.x - parent.x, local.y - parent.y, local.z - parent.z};
        }
        FbxNode& bone = objects.emplace_back(FbxNode{"Model", {P::i64(joint_ids[joint]),
            P::string(object_name(record.name, "Model")), P::string("LimbNode")}, {}});
        bone.add("Version", {P::i32(232)});
        FbxNode& p70 = bone.add("Properties70");
        property_vector(p70, "Lcl Translation", "Lcl Translation", "", "A", local.x, local.y, local.z);
        property_vector(p70, "Lcl Rotation", "Lcl Rotation", "", "A", 0, 0, 0);
        property_vector(p70, "Lcl Scaling", "Lcl Scaling", "", "A", 1, 1, 1);
        model_tail(bone, p70);
        ++counts["Model"];
        connect(attribute, joint_ids[joint]);
    }
    for (size_t joint = 0; joint < model.joints.size(); ++joint)
    {
        const int parent = model.joints[joint].parent;
        connect(joint_ids[joint], parent >= 0 && size_t(parent) < model.joints.size() ? joint_ids[size_t(parent)] : 0);
    }

    // Textures: PNG files in <name>.fbm beside the FBX, and embedded.
    const std::filesystem::path folder = out.parent_path() / (out.stem().string() + ".fbm");
    std::map<i32, std::int64_t> texture_ids;
    for (const ModelMaterialInfo& material : model.materials)
    {
        const i32 index = material.colour_texture;
        if (index < 0 || size_t(index) >= model.textures.size() || texture_ids.contains(index)
            || model.textures[size_t(index)].image.bgra.empty())
        {
            continue;
        }
        const std::string file_name = "texture" + std::to_string(index) + ".png";
        const auto png = encode_png(model.textures[size_t(index)].image);
        std::filesystem::create_directories(folder);
        write_file(folder / file_name, png);
        const std::string relative = (std::filesystem::path(out.stem().string() + ".fbm") / file_name).string();
        const std::string absolute = std::filesystem::absolute(folder / file_name).string();
        const std::int64_t video = id();
        FbxNode& clip = objects.emplace_back(FbxNode{"Video", {P::i64(video), P::string(object_name(file_name, "Video")),
            P::string("Clip")}, {}});
        clip.add("Type", {P::string("Clip")});
        FbxNode& video_p70 = clip.add("Properties70");
        property(video_p70, "Path", "KString", "XRefUrl", "").properties.push_back(P::string(absolute));
        clip.add("UseMipMap", {P::i32(0)});
        clip.add("Filename", {P::string(absolute)});
        clip.add("RelativeFilename", {P::string(relative)});
        clip.add("Content", {P::raw(std::string(png.begin(), png.end()))});
        ++counts["Video"];
        const std::int64_t texture = id();
        FbxNode& node = objects.emplace_back(FbxNode{"Texture", {P::i64(texture), P::string(object_name(file_name,
            "Texture")), P::string("")}, {}});
        node.add("Type", {P::string("TextureVideoClip")});
        node.add("Version", {P::i32(202)});
        node.add("TextureName", {P::string(object_name(file_name, "Texture"))});
        FbxNode& texture_p70 = node.add("Properties70");
        property(texture_p70, "UseMaterial", "bool", "", "").properties.push_back(P::i32(1));
        node.add("Media", {P::string(object_name(file_name, "Video"))});
        node.add("FileName", {P::string(absolute)});
        node.add("RelativeFilename", {P::string(relative)});
        node.add("ModelUVTranslation", {P::f64(0), P::f64(0)});
        node.add("ModelUVScaling", {P::f64(1), P::f64(1)});
        node.add("Texture_Alpha_Source", {P::string("None")});
        node.add("Cropping", {P::i32(0), P::i32(0), P::i32(0), P::i32(0)});
        ++counts["Texture"];
        connect(video, texture);
        texture_ids.emplace(index, texture);
    }
    // Materials, by the template's material node.
    std::map<u32, std::int64_t> material_ids;
    for (const ModelMaterialInfo& material : model.materials)
    {
        const std::int64_t material_id = id();
        FbxNode& node = objects.emplace_back(FbxNode{"Material", {P::i64(material_id), P::string(object_name(
            material.name, "Material")), P::string("")}, {}});
        node.add("Version", {P::i32(102)});
        node.add("ShadingModel", {P::string("phong")});
        node.add("MultiLayer", {P::i32(0)});
        FbxNode& p70 = node.add("Properties70");
        property_vector(p70, "DiffuseColor", "Color", "", "A", 1, 1, 1);
        property_double(p70, "DiffuseFactor", 1.0);
        ++counts["Material"];
        if (const auto texture = texture_ids.find(material.colour_texture); texture != texture_ids.end())
        {
            connect_property(texture->second, material_id, "DiffuseColor");
        }
        material_ids.emplace(material.node, material_id);
    }

    // Meshes.
    const std::int64_t pose = id();
    FbxNode bind_pose{"Pose", {P::i64(pose), P::string(object_name("BindPose", "Pose")), P::string("BindPose")}, {}};
    bind_pose.add("Type", {P::string("BindPose")});
    bind_pose.add("Version", {P::i32(100)});
    std::vector<FbxNode> pose_nodes;
    for (size_t joint = 0; joint < model.joints.size(); ++joint)
    {
        FbxNode node{"PoseNode", {}, {}};
        node.add("Node", {P::i64(joint_ids[joint])});
        node.add("Matrix", {P::f64s(translation_matrix(model.joints[joint].world))});
        pose_nodes.push_back(std::move(node));
    }
    for (const bool head : {false, true})
    {
        std::vector<const ModelPrimitive*> parts;
        for (const ModelPrimitive& primitive : model.primitives)
        {
            if (primitive.head == head && !primitive.indices.empty())
            {
                parts.push_back(&primitive);
            }
        }
        if (parts.empty())
        {
            continue;
        }
        const std::string name = head ? "head" : "body";
        std::vector<double> vertices, normals, uvs, colours;
        std::vector<std::int64_t> polygons, uv_index, colour_index, materials;
        std::vector<std::int64_t> mesh_materials;
        std::map<u16, std::pair<std::vector<std::int64_t>, std::vector<double>>> influences;
        size_t base = 0;
        for (const ModelPrimitive* primitive : parts)
        {
            const std::int64_t material = material_ids.at(primitive->material_node);
            auto known = std::find(mesh_materials.begin(), mesh_materials.end(), material);
            if (known == mesh_materials.end())
            {
                mesh_materials.push_back(material);
                known = mesh_materials.end() - 1;
            }
            const std::int64_t material_index = known - mesh_materials.begin();
            for (size_t vertex = 0; vertex < primitive->vertices.size(); ++vertex)
            {
                const ModelVertex& v = primitive->vertices[vertex];
                vertices.insert(vertices.end(), {v.position.x, v.position.y, v.position.z});
                // FBX's texture origin is the bottom left.
                uvs.insert(uvs.end(), {v.uv.u, 1.0 - v.uv.v});
                colours.insert(colours.end(), {double(u8(v.colour >> 16)) / 255.0, double(u8(v.colour >> 8)) / 255.0,
                    double(u8(v.colour)) / 255.0, double(u8(v.colour >> 24)) / 255.0});
                const float total = v.weights[0] + v.weights[1];
                const std::int64_t control = std::int64_t(base + vertex);
                influences[v.joints[0]].first.push_back(control);
                influences[v.joints[0]].second.push_back(v.weights[0] / total);
                if (v.joints[1] != v.joints[0] && v.weights[1] > 0.f)
                {
                    influences[v.joints[1]].first.push_back(control);
                    influences[v.joints[1]].second.push_back(v.weights[1] / total);
                }
            }
            for (size_t index = 0; index + 2 < primitive->indices.size(); index += 3)
            {
                for (int corner = 0; corner < 3; ++corner)
                {
                    const std::int64_t control = std::int64_t(base + primitive->indices[index + size_t(corner)]);
                    // The last index of a polygon is stored as -(index + 1).
                    polygons.push_back(corner == 2 ? -control - 1 : control);
                    const Vec3& n = primitive->vertices[primitive->indices[index + size_t(corner)]].normal;
                    normals.insert(normals.end(), {n.x, n.y, n.z});
                    uv_index.push_back(control);
                    colour_index.push_back(control);
                }
                materials.push_back(material_index);
            }
            base += primitive->vertices.size();
        }
        const std::int64_t geometry = id();
        FbxNode& mesh = objects.emplace_back(FbxNode{"Geometry", {P::i64(geometry), P::string(object_name(name,
            "Geometry")), P::string("Mesh")}, {}});
        mesh.add("Properties70");
        mesh.add("GeometryVersion", {P::i32(124)});
        mesh.add("Vertices", {P::f64s(vertices)});
        mesh.add("PolygonVertexIndex", {P::i32s(polygons)});
        FbxNode& normal_layer = mesh.add("LayerElementNormal", {P::i32(0)});
        normal_layer.add("Version", {P::i32(101)});
        normal_layer.add("Name", {P::string("")});
        normal_layer.add("MappingInformationType", {P::string("ByPolygonVertex")});
        normal_layer.add("ReferenceInformationType", {P::string("Direct")});
        normal_layer.add("Normals", {P::f64s(normals)});
        FbxNode& colour_layer = mesh.add("LayerElementColor", {P::i32(0)});
        colour_layer.add("Version", {P::i32(101)});
        colour_layer.add("Name", {P::string("colour")});
        colour_layer.add("MappingInformationType", {P::string("ByPolygonVertex")});
        colour_layer.add("ReferenceInformationType", {P::string("IndexToDirect")});
        colour_layer.add("Colors", {P::f64s(colours)});
        colour_layer.add("ColorIndex", {P::i32s(colour_index)});
        FbxNode& uv_layer = mesh.add("LayerElementUV", {P::i32(0)});
        uv_layer.add("Version", {P::i32(101)});
        uv_layer.add("Name", {P::string("UVMap")});
        uv_layer.add("MappingInformationType", {P::string("ByPolygonVertex")});
        uv_layer.add("ReferenceInformationType", {P::string("IndexToDirect")});
        uv_layer.add("UV", {P::f64s(uvs)});
        uv_layer.add("UVIndex", {P::i32s(uv_index)});
        FbxNode& material_layer = mesh.add("LayerElementMaterial", {P::i32(0)});
        material_layer.add("Version", {P::i32(101)});
        material_layer.add("Name", {P::string("")});
        material_layer.add("MappingInformationType", {P::string("ByPolygon")});
        material_layer.add("ReferenceInformationType", {P::string("IndexToDirect")});
        material_layer.add("Materials", {P::i32s(materials)});
        FbxNode& layer = mesh.add("Layer", {P::i32(0)});
        layer.add("Version", {P::i32(100)});
        for (const char* type : {"LayerElementNormal", "LayerElementColor", "LayerElementUV", "LayerElementMaterial"})
        {
            FbxNode& element = layer.add("LayerElement");
            element.add("Type", {P::string(type)});
            element.add("TypedIndex", {P::i32(0)});
        }
        ++counts["Geometry"];
        const std::int64_t mesh_model = id();
        FbxNode& node = objects.emplace_back(FbxNode{"Model", {P::i64(mesh_model), P::string(object_name(name, "Model")),
            P::string("Mesh")}, {}});
        node.add("Version", {P::i32(232)});
        FbxNode& model_p70 = node.add("Properties70");
        property_vector(model_p70, "Lcl Translation", "Lcl Translation", "", "A", 0, 0, 0);
        property_vector(model_p70, "Lcl Rotation", "Lcl Rotation", "", "A", 0, 0, 0);
        property_vector(model_p70, "Lcl Scaling", "Lcl Scaling", "", "A", 1, 1, 1);
        model_tail(node, model_p70);
        ++counts["Model"];
        connect(mesh_model, 0);
        connect(geometry, mesh_model);
        for (const std::int64_t material : mesh_materials)
        {
            connect(material, mesh_model);
        }
        FbxNode mesh_pose{"PoseNode", {}, {}};
        mesh_pose.add("Node", {P::i64(mesh_model)});
        mesh_pose.add("Matrix", {P::f64s(translation_matrix({}))});
        pose_nodes.push_back(std::move(mesh_pose));
        // The skin: one cluster per joint that moves this mesh.
        const std::int64_t skin = id();
        FbxNode& skin_node = objects.emplace_back(FbxNode{"Deformer", {P::i64(skin), P::string(object_name("Skin " + name,
            "Deformer")), P::string("Skin")}, {}});
        skin_node.add("Version", {P::i32(101)});
        skin_node.add("Link_DeformAcuracy", {P::f64(50.0)});
        ++counts["Deformer"];
        connect(skin, geometry);
        for (const auto& [joint, list] : influences)
        {
            const std::int64_t cluster = id();
            FbxNode& cluster_node = objects.emplace_back(FbxNode{"Deformer", {P::i64(cluster), P::string(object_name(
                "Cluster " + model.joints[joint].name, "SubDeformer")), P::string("Cluster")}, {}});
            cluster_node.add("Version", {P::i32(100)});
            cluster_node.add("UserData", {P::string(""), P::string("")});
            cluster_node.add("Indexes", {P::i32s(list.first)});
            cluster_node.add("Weights", {P::f64s(list.second)});
            const Vec3& world = model.joints[joint].world;
            cluster_node.add("Transform", {P::f64s(translation_matrix({-world.x, -world.y, -world.z}))});
            cluster_node.add("TransformLink", {P::f64s(translation_matrix(world))});
            ++counts["Deformer"];
            connect(cluster, skin);
            connect(joint_ids[joint], cluster);
        }
        // The head's shapes.
        if (head && !model.morph_names.empty())
        {
            const std::int64_t blend = id();
            FbxNode& blend_node = objects.emplace_back(FbxNode{"Deformer", {P::i64(blend), P::string(object_name(name,
                "Deformer")), P::string("BlendShape")}, {}});
            blend_node.add("Version", {P::i32(100)});
            ++counts["Deformer"];
            connect(blend, geometry);
            for (size_t target = 0; target < model.morph_names.size(); ++target)
            {
                std::vector<std::int64_t> indexes;
                std::vector<double> deltas, normal_deltas;
                size_t offset = 0;
                for (const ModelPrimitive* primitive : parts)
                {
                    for (size_t vertex = 0; vertex < primitive->vertices.size(); ++vertex)
                    {
                        const Vec3 delta = target < primitive->morphs.size() ? primitive->morphs[target][vertex] : Vec3{};
                        if (delta.x != 0.f || delta.y != 0.f || delta.z != 0.f)
                        {
                            indexes.push_back(std::int64_t(offset + vertex));
                            deltas.insert(deltas.end(), {delta.x, delta.y, delta.z});
                            normal_deltas.insert(normal_deltas.end(), {0.0, 0.0, 0.0});
                        }
                    }
                    offset += primitive->vertices.size();
                }
                const std::string& morph = model.morph_names[target];
                const std::int64_t channel = id();
                FbxNode& channel_node = objects.emplace_back(FbxNode{"Deformer", {P::i64(channel), P::string(object_name(
                    morph, "SubDeformer")), P::string("BlendShapeChannel")}, {}});
                channel_node.add("Version", {P::i32(100)});
                channel_node.add("DeformPercent", {P::f64(0.0)});
                channel_node.add("FullWeights", {P::f64s({100.0})});
                ++counts["Deformer"];
                const std::int64_t shape = id();
                FbxNode& shape_node = objects.emplace_back(FbxNode{"Geometry", {P::i64(shape), P::string(object_name(
                    morph, "Geometry")), P::string("Shape")}, {}});
                shape_node.add("Properties70");
                shape_node.add("Version", {P::i32(100)});
                shape_node.add("Indexes", {P::i32s(indexes)});
                shape_node.add("Vertices", {P::f64s(deltas)});
                shape_node.add("Normals", {P::f64s(normal_deltas)});
                ++counts["Geometry"];
                connect(shape, channel);
                connect(channel, blend);
            }
        }
    }
    bind_pose.add("NbPoseNodes", {P::i32(std::int64_t(pose_nodes.size()))});
    for (FbxNode& node : pose_nodes)
    {
        bind_pose.children.push_back(std::move(node));
    }
    objects.push_back(std::move(bind_pose));
    ++counts["Pose"];

    // Definitions: how many objects of each type.
    FbxNode& definitions = top.emplace_back(FbxNode{"Definitions", {}, {}});
    definitions.add("Version", {P::i32(100)});
    int total = 1;
    for (const auto& [type, count] : counts)
    {
        total += count;
    }
    definitions.add("Count", {P::i32(total)});
    definitions.add("ObjectType", {P::string("GlobalSettings")}).add("Count", {P::i32(1)});
    for (const auto& [type, count] : counts)
    {
        definitions.add("ObjectType", {P::string(type)}).add("Count", {P::i32(count)});
    }
    top.push_back(FbxNode{"Objects", {}, std::move(objects)});
    top.push_back(FbxNode{"Connections", {}, std::move(connections)});
    FbxNode& takes = top.emplace_back(FbxNode{"Takes", {}, {}});
    takes.add("Current", {P::string("")});
    if (!out.parent_path().empty())
    {
        std::filesystem::create_directories(out.parent_path());
    }
    write_file(out, write_fbx(top));
}
}
#endif
