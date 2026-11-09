#if defined(GBTG_INSPECTOR_APP) && !defined(GBTG_RETAIL_GAME_ONLY)
#include "model_io.h"
#include "scene_build_tool.h"
#include "gbtg/aid.h"
#include "gbtg/model_nodes.h"
#include "fbx_export.h"
#include "fbx_import.h"
#include "gltf_export.h"
#include "model_import.h"
#include "gbtg/resource.h"
#include "serialized_model.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <sstream>

namespace gbtg
{
namespace
{
std::string hex_words(const SerializedImage& image, u32 at, u32 count)
{
    std::string out;
    char text[16];
    for (u32 index = 0; index < count; ++index)
    {
        std::snprintf(text, sizeof(text), "%08X ", image.word(at, index * 4));
        out += text;
    }
    return out;
}

void describe_material(std::ostringstream& out, const SerializedImage& image, u32 tree, u32 material,
    const std::string& indent)
{
    using namespace model_material_field;
    const u32 stages_count = image.get(material, model_material_field::stage_count);
    out << indent << "  material @" << std::hex << material << std::dec << " stages " << stages_count << " constants "
        << image.get(material, constant_count) << " colours " << image.get(material, colour_count) << " alpha_ref "
        << int(image.get(material, alpha_reference)) << " instance_size " << image.get(material, instance_size);
    if (image.get(material, name) != 0)
    {
        out << " name '" << image.text(tree + image.get(material, name)) << "'";
    }
    out << "\n";
    const u32 stages = image.get(material, model_material_field::stages);
    for (u32 stage = 0; stage < stages_count && stages != 0; ++stage)
    {
        const u32 record = tree + stages + stage * model_material_stage_field::kSize;
        out << indent << "    stage " << stage << " texture " << i32(image.get(record, model_material_stage_field::texture))
            << " uv-address " << int(image.get(record, model_material_stage_field::address_u)) << ","
            << int(image.get(record, model_material_stage_field::address_v)) << " keyed "
            << int(image.get(record, model_material_stage_field::keyed)) << "\n";
    }
    const u32 entries = image.get(material, model_material_field::entries);
    for (u32 entry = 0; entry < image.get(material, entry_count) && entries != 0; ++entry)
    {
        const u32 record = tree + entries + entry * model_material_entry_field::kSize;
        out << indent << "    entry '" << image.text(tree + image.get(record, model_material_entry_field::name))
            << "' = " << image.get(record, model_material_entry_field::value) << "\n";
    }
}

void describe_node_chain(std::ostringstream& out, const SerializedImage& image, u32 tree, u32 node, int depth)
{
    for (; node != 0; node = image.get(tree + node, model_node_field::next))
    {
        const u32 at = tree + node;
        const std::string indent(size_t(depth) * 2, ' ');
        const std::string type = image.text(tree + image.word(at));
        out << indent << type << " @" << std::hex << node << std::dec << " mask " << image.get(at, model_node_field::mask)
            << " instance+" << std::hex << image.get(at, model_node_field::instance_offset) << std::dec;
        if (type == "ndVertexBuffer")
        {
            const u32 count = image.get(at, vertex_buffer_node::buffer_count);
            out << " buffers " << count << "\n";
            for (u32 index = 0; index < count; ++index)
            {
                const u32 record = tree + image.get(at, vertex_buffer_node::buffers) + index * model_vertex_buffer_field::kSize;
                out << indent << "  buffer " << index << ": " << hex_words(image, record, 6) << "\n";
            }
        }
        else if (type == "ndVertexShader" || type == "ndVtxDesc")
        {
            using namespace vertex_shader_node;
            const u32 streams_count = image.get(at, stream_count);
            out << " streams " << streams_count << " regs proj " << image.get(at, projection_register) << " world "
                << image.get(at, world_register) << " cam " << image.get(at, camera_register) << " param "
                << image.get(at, parameter_register) << " aux " << image.get(at, auxiliary_register) << " ch "
                << i32(image.get(at, first_channel)) << "," << i32(image.get(at, second_channel)) << " variant "
                << image.get(at, variant) << "\n";
            const u32 stream_list = tree + image.get(at, streams);
            const u32 stride_list = tree + image.get(at, strides);
            out << indent << "  streams:";
            for (u32 index = 0; index < streams_count; ++index)
            {
                out << " " << image.word(stream_list, index * 4) << "/stride " << int(image.byte(stride_list, index));
            }
            out << "\n" << indent << "  declaration:";
            const u32 declaration_at = tree + image.get(at, declaration);
            for (u32 index = 0; index < 64; ++index)
            {
                const u32 value = image.word(declaration_at, index * 4);
                char text[16];
                std::snprintf(text, sizeof(text), " %08X", value);
                out << text;
                if (value == 0xFFFFFFFF)
                {
                    break;
                }
            }
            const u32 program = tree + image.get(at, vertex_shader_node::program);
            out << "\n" << indent << "  program: " << image.half(program, 2) << " instructions, header "
                << hex_words(image, program, 1) << "\n";
        }
        else if (type == "ndPushBuffer" || type == "ndBGPushBuffer")
        {
            using namespace push_buffer_node;
            out << " draws " << image.get(at, draw_count) << " header " << std::hex << image.get(at, header) << std::dec
                << " size " << image.get(at, size) << " cull_off " << int(image.get(at, cull_disable)) << " state "
                << int(std::int8_t(image.get(at, state_slot))) << "=" << int(image.get(at, state_value)) << "\n";
            if (image.get(at, header) == 0)
            {
                for (u32 draw = 0; draw < image.get(at, draw_count); ++draw)
                {
                    out << indent << "  draw " << draw << ": indices @" << std::hex
                        << image.word(tree + image.get(at, draw_indices), draw * 4) << std::dec << " primitive "
                        << image.word(tree + image.get(at, draw_primitives), draw * 4) << " count "
                        << image.word(tree + image.get(at, draw_counts), draw * 4) << "\n";
                }
            }
        }
        else if (type == "ndSkeleton")
        {
            out << " joints " << image.get(at, skeleton_node::joint_count) << "\n";
        }
        else if (type == "ndMtxArray")
        {
            using namespace matrix_array_node;
            const i32 rows_count = image.get(at, row_count);
            out << " rows " << rows_count << " matrices " << image.get(at, matrix_count) << "\n" << indent << "  rows:";
            for (i32 row = 0; row < rows_count; ++row)
            {
                const u32 record = tree + image.get(at, rows) + u32(row) * 8;
                out << " [" << image.half(record) << "," << image.half(record, 2) << "," << image.half(record, 4) << ","
                    << int(image.byte(record, 6)) << "," << int(image.byte(record, 7)) << "]";
            }
            out << "\n";
        }
        else if (type == "ndRigidSkinIdx")
        {
            const u32 count = image.get(at, rigid_skin_node::index_count);
            out << " indices " << count << ":";
            for (u32 index = 0; index < count; ++index)
            {
                out << " " << int(image.byte(tree + image.get(at, rigid_skin_node::indices), index));
            }
            out << "\n";
        }
        else if (type == "ndShaderParam2")
        {
            out << "\n";
            describe_material(out, image, tree, tree + image.get(at, shader_node::primary), indent);
            if (image.get(at, shader_node::secondary) != 0)
            {
                describe_material(out, image, tree, tree + image.get(at, shader_node::secondary), indent);
            }
        }
        else if (type == "ndBlendShape")
        {
            using namespace blendshape_node;
            const u32 count = image.get(at, target_count);
            out << " targets " << count << " blends " << int(image.get(at, blends)) << "\n";
            for (u32 target = 0; target < count; ++target)
            {
                const u32 table = tree + image.word(tree + image.get(at, targets), target * 4);
                const u32 buffers = image.get(table, vertex_buffer_node::buffer_count);
                out << indent << "  target " << target << ": " << buffers << " buffers";
                for (u32 index = 0; index < buffers; ++index)
                {
                    const u32 record = tree + image.get(table, vertex_buffer_node::buffers)
                        + index * model_vertex_buffer_field::kSize;
                    out << " {" << hex_words(image, record, 6) << "}";
                }
                out << "\n";
            }
        }
        else if (type == "ndTransform" || type == "nd2D")
        {
            out << " mode " << int(image.get(at, transform_node::mode)) << " joint " << image.get(at, transform_node::joint)
                << "\n";
        }
        else
        {
            out << "\n";
        }
        const u32 child = image.get(at, model_node_field::child);
        if (child != 0)
        {
            describe_node_chain(out, image, tree, child, depth + 1);
        }
    }
}

bool is_fbx(const std::filesystem::path& path)
{
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c)
    {
        return char(std::tolower(c));
    });
    return extension == ".fbx";
}

const BundleEntry* find_model_entry(AidStore& store, const std::string& aid, const std::filesystem::path& game_dir)
{
    if (const BundleEntry* entry = store.load_aid(aid))
    {
        return entry;
    }
    AidIndex index;
    index.load(game_dir, "cache/aid_index.tsv");
    if (const auto* places = index.find(aid))
    {
        for (const auto& place : *places)
        {
            store.load_bundle_rel(place.bundle);
        }
    }
    return store.load_aid(aid);
}
}

std::string describe_serialized_model(const BundleEntry& entry)
{
    const SerializedImage image{entry.cpu};
    std::ostringstream out;
    out << entry.name << ": CPU " << entry.cpu.size() << " bytes, GPU " << entry.gpu.size() << " bytes in "
        << entry.gpu_chunks.size() << " chunks";
    for (const auto& chunk : entry.gpu_chunks)
    {
        out << " [" << chunk.offset << "+" << chunk.size << "]";
    }
    out << "\nCPU header: " << hex_words(image, 0, 8) << "\n";
    const u32 entries = image.word(0);
    const u32 count = image.word(0, 4);
    for (u32 index = 0; index < count; ++index)
    {
        const u32 kind = image.word(entries, index * 8);
        const u32 block = image.word(entries, index * 8 + 4);
        out << "directory " << index << ": kind 0x" << std::hex << kind << " block @" << block << std::dec << "\n";
        if (kind == 0)
        {
            const u32 tree = image.get(block, model_tree_block_field::tree);
            out << "  tree @" << std::hex << tree << " gpu_base " << image.get(block, model_tree_block_field::gpu_base)
                << std::dec << " instance_size " << image.word(tree) << " roots "
                << image.get(tree, model_tree_field::root_count) << " names @" << std::hex
                << image.get(tree, model_tree_field::names) << " lists @" << image.get(tree, model_tree_field::lists)
                << " second_lists @" << image.get(tree, model_tree_field::second_lists) << std::dec << "\n";
            const u32 names = image.get(tree, model_tree_field::names);
            if (names != 0)
            {
                for (u32 name = 0; name < image.word(tree + names); ++name)
                {
                    const u32 record = tree + image.word(tree + names, 4) + name * 0xC;
                    out << "  name '" << image.text(tree + image.word(record)) << "' -> @" << std::hex
                        << image.word(record, 4) << " (" << image.word(tree + image.word(record, 4)) << ")" << std::dec
                        << "\n";
                }
            }
            for (u32 root = 0; root < image.get(tree, model_tree_field::root_count); ++root)
            {
                out << "  root " << root << ":\n";
                describe_node_chain(out, image, tree, image.word(tree + image.get(tree, model_tree_field::roots), root * 4),
                    2);
            }
        }
        else if (kind == 7)
        {
            const u32 textures = image.word(block);
            out << "  textures " << textures << "\n";
            for (u32 texture = 0; texture < textures; ++texture)
            {
                const u32 record = image.word(image.get(block, model_texture_table_field::textures), texture * 4);
                out << "  texture " << texture << " @" << std::hex << record << ": format "
                    << image.get(record, model_texture_field::format) << std::dec << " "
                    << image.get(record, model_texture_field::width) << "x" << image.get(record, model_texture_field::height)
                    << " levels " << int(image.get(record, model_texture_field::levels)) << " frames "
                    << int(image.get(record, model_texture_field::frame_count)) << " data " << std::hex
                    << image.get(record, model_texture_field::data) << std::dec << " frame_size "
                    << image.get(record, model_texture_field::frame_size) << "\n";
            }
        }
    }
    return out.str();
}

DiscModel load_disc_model(const std::string& aid, const std::filesystem::path& game_dir)
{
    AidIndex index;
    index.load(game_dir, "cache/aid_index.tsv");
    std::vector<std::string> bundles{"common.bnl"};
    if (const auto* places = index.find(aid))
    {
        for (const auto& place : *places)
        {
            bundles.push_back(place.bundle);
        }
    }
    for (const auto& relative : bundles)
    {
        const std::filesystem::path path = game_dir / "bundles" / std::filesystem::path(relative);
        if (!std::filesystem::exists(path))
        {
            continue;
        }
        DiscModel model;
        model.bundle = load_bundle(path); // The disc's bytes: no plugin replaces them here.
        if (const BundleEntry* entry = find_entry(model.bundle, aid); entry != nullptr && entry->type == ResourceType::Model)
        {
            model.entry = entry;
            return model;
        }
    }
    throw Error("no disc bundle holds the model " + aid);
}

namespace
{
std::filesystem::path g_inspector_executable;
std::filesystem::path g_inspector_game_dir;

// Beside an export: <stem>.gbtg.ini (the model, its materials by part and their textures) and every texture of the
// model in <stem>.fbm, for the 3ds Max tools (tools/max/gbtg_tools.ms) to build game materials from.
void write_manifest(const DecodedModel& model, const std::filesystem::path& out)
{
    const std::filesystem::path folder = out.parent_path() / (out.stem().string() + ".fbm");
    std::filesystem::create_directories(folder);
    for (const auto& texture : model.textures)
    {
        if (!texture.image.bgra.empty())
        {
            write_png(folder / ("texture" + std::to_string(texture.index) + ".png"), texture.image);
        }
    }
    const auto file = [&model](i32 index) -> std::string
    {
        return index >= 0 && size_t(index) < model.textures.size() && !model.textures[size_t(index)].image.bgra.empty()
            ? "texture" + std::to_string(index) + ".png" : "";
    };
    std::ostringstream ini;
    ini << "; Written by gbtg-inspector beside a model export, for the 3ds Max tools. Holds no disc data.\n"
        << "[Model]\naid=" << model.aid << "\nfile=" << out.filename().string() << "\ntextures="
        << folder.filename().string() << "\ninspector=" << g_inspector_executable.string() << "\ngame_dir="
        << (g_inspector_game_dir.empty() ? std::string{} : std::filesystem::absolute(g_inspector_game_dir).string())
        << "\n";
    std::vector<std::pair<std::string, bool>> seen;
    std::ostringstream materials;
    int count = 0;
    for (const ModelPrimitive& primitive : model.primitives)
    {
        const auto key = std::pair{primitive.material, primitive.head};
        if (std::find(seen.begin(), seen.end(), key) != seen.end())
        {
            continue;
        }
        seen.push_back(key);
        const auto info = std::find_if(model.materials.begin(), model.materials.end(), [&](const ModelMaterialInfo& m)
        {
            return m.node == primitive.material_node;
        });
        ++count;
        materials << "[Material" << count << "]\nname=" << primitive.material << "\npart="
                  << (primitive.head ? "head" : "body");
        if (info != model.materials.end())
        {
            materials << "\ncolour=" << file(info->colour_texture) << "\ncelmap=" << file(info->celmap_texture)
                      << "\nlightmap=" << file(info->lightmap_texture);
            if (info->colour_texture >= 0 && size_t(info->colour_texture) < model.textures.size())
            {
                const auto& texture = model.textures[size_t(info->colour_texture)];
                materials << "\nsize=" << texture.width << "x" << texture.height;
            }
        }
        materials << "\n";
    }
    ini << "[Materials]\ncount=" << count << "\n" << materials.str();
    const std::string text = ini.str();
    write_file(out.parent_path() / (out.stem().string() + ".gbtg.ini"),
        std::span<const u8>(reinterpret_cast<const u8*>(text.data()), text.size()));
}
}

void set_model_io_host(const std::filesystem::path& executable, const std::filesystem::path& game_dir)
{
    g_inspector_executable = executable;
    g_inspector_game_dir = game_dir;
}

std::string export_model_file(const BundleEntry& entry, const std::filesystem::path& out)
{
    if (entry.type != ResourceType::Model)
    {
        throw Error(entry.name + " is not a model");
    }
    const DecodedModel model = decode_serialized_model(entry);
    if (!out.parent_path().empty())
    {
        std::filesystem::create_directories(out.parent_path());
    }
    if (is_fbx(out))
    {
        write_fbx_model(model, out);
    }
    else
    {
        write_file(out, write_gltf(model));
    }
    write_manifest(model, out);
    size_t vertices = 0;
    size_t triangles = 0;
    for (const auto& primitive : model.primitives)
    {
        vertices += primitive.vertices.size();
        triangles += primitive.indices.size() / 3;
    }
    return "Exported " + entry.name + " to " + out.string() + ": " + std::to_string(model.joints.size()) + " joints, "
        + std::to_string(model.primitives.size()) + " primitives, " + std::to_string(vertices) + " vertices, "
        + std::to_string(triangles) + " triangles, " + std::to_string(model.morph_names.size()) + " morph targets.";
}

std::filesystem::path import_model_file(const std::filesystem::path& glb, const std::string& aid,
    const std::filesystem::path& game_dir, const std::filesystem::path& plugin, std::vector<std::string>& notes)
{
    const DiscModel disc = load_disc_model(aid, game_dir);
    const GltfScene scene = is_fbx(glb) ? read_fbx_scene(glb) : read_glb(glb);
    ModelImportResult result = import_model(*disc.entry, scene);
    // Read it back as the loader receives it, and decode it again.
    const LooseResource written = parse_loose_resource(result.resource);
    BundleEntry check;
    check.name = aid;
    check.type = written.header.type;
    check.header = written.header;
    check.gpu_chunks = written.gpu_chunks;
    check.cpu = written.cpu;
    check.gpu = written.gpu;
    const DecodedModel decoded = decode_serialized_model(check);
    size_t triangles = 0;
    for (const auto& primitive : decoded.primitives)
    {
        triangles += primitive.indices.size() / 3;
    }
    const std::filesystem::path out = plugin / "aid" / aid;
    std::filesystem::create_directories(out.parent_path());
    // Written beside and renamed over, so a watching inspector never reads half a file.
    const std::filesystem::path partial = out.string() + ".partial";
    write_file(partial, result.resource);
    std::filesystem::rename(partial, out);
    if (!std::filesystem::exists(plugin / "plugin.ini"))
    {
        const std::string ini = "[Plugin]\nName = " + plugin.filename().string() + "\nVersion = 1.0\nEnabled = 1\n";
        write_file(plugin / "plugin.ini", std::span<const u8>(reinterpret_cast<const u8*>(ini.data()), ini.size()));
    }
    notes = std::move(result.notes);
    notes.push_back("Wrote " + out.string() + " (" + std::to_string(result.resource.size() / 1024) + " KiB); it decodes "
        "to " + std::to_string(decoded.primitives.size()) + " primitives and " + std::to_string(triangles)
        + " triangles. It holds disc data: keep it local, do not share or commit it.");
    return out;
}

int import_model_command(const std::filesystem::path& glb, const std::string& aid, const std::filesystem::path& game_dir,
    const std::filesystem::path& plugin)
{
    std::vector<std::string> notes;
    import_model_file(glb, aid, game_dir, plugin, notes);
    for (const auto& note : notes)
    {
        std::cout << note << "\n";
    }
    return 0;
}

int export_textures_command(const std::string& aid, const std::filesystem::path& game_dir,
    const std::filesystem::path& out)
{
    const DiscModel disc = load_disc_model(aid, game_dir);
    const DecodedModel model = decode_serialized_model(*disc.entry);
    std::filesystem::create_directories(out);
    for (const auto& texture : model.textures)
    {
        if (texture.image.bgra.empty())
        {
            continue;
        }
        std::string uses;
        for (const auto& material : model.materials)
        {
            const char* role = material.colour_texture == i32(texture.index) ? "colour"
                : material.celmap_texture == i32(texture.index) ? "celmap"
                : material.lightmap_texture == i32(texture.index) ? "lightmap" : nullptr;
            if (role != nullptr && uses.find(material.name + "_" + role) == std::string::npos)
            {
                uses += "_" + material.name + "_" + role;
            }
        }
        const std::string name = "texture" + std::to_string(texture.index) + uses + ".png";
        write_png(out / name, texture.image);
        std::cout << name << " " << texture.width << "x" << texture.height << " " << xbox_format_name(XboxFormat(texture.format))
                  << "\n";
    }
    return 0;
}

int export_model_command(const std::string& aid, const std::filesystem::path& game_dir,
    const std::filesystem::path& out)
{
    const DiscModel disc = load_disc_model(aid, game_dir);
    std::cout << export_model_file(*disc.entry, out) << "\n";
    return 0;
}

int describe_model_command(const std::string& aid, const std::filesystem::path& game_dir)
{
    AidStore store;
    store.open(game_dir);
    const BundleEntry* entry = find_model_entry(store, aid, game_dir);
    if (entry == nullptr)
    {
        std::cerr << "no resource " << aid << "\n";
        return 1;
    }
    std::cout << describe_serialized_model(*entry);
    return 0;
}
}
#endif
