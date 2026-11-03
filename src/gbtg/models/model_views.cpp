#include "gbtg/model_views.h"
#include <bit>
#include <cstring>
#include <deque>

namespace gbtg
{

struct RuntimeModelAssetView::Storage
{
    struct Shader
    {
        RuntimeShaderCpu cpu;
        std::vector<RuntimeShaderCpuEntry> entries;
        std::vector<std::array<u8, 0x48>> parameters;
    };
    struct Blendshape
    {
        RuntimeBlendshapeCpu cpu;
        std::vector<RuntimeBlendshapeVertexBuffer> buffers;
    };
    std::deque<RuntimeModelNode> nodes;
    std::deque<Shader> shaders;
    std::deque<Blendshape> blendshapes;
    std::deque<std::vector<Joint>> skeletons;
    std::unordered_map<u32, RuntimeModelNode*> node_addresses;
    std::vector<RuntimeModelNode*> roots;
    std::vector<RuntimeModelNamedEntry> names;
    RuntimeModelNameTable name_table{};
    RuntimeModelTree tree{};
    std::vector<RuntimeModelResourceBlock> blocks;
};

RuntimeModelAssetView::RuntimeModelAssetView() : storage(std::make_unique<Storage>())
{
}

RuntimeModelAssetView::~RuntimeModelAssetView() = default;

namespace
{

class ViewDecoder
{
public:
    RuntimeModelAssetView::Storage& storage;
    const RuntimeRelocationImage& image;
    std::span<const RuntimeModelNodeType> copy_types;
    u32 descriptor_base;
    const std::function<const u8*(u32)>& gpu_pointer;

    const RuntimeShaderCpu* shader(u32 address)
    {
        auto& output = storage.shaders.emplace_back();
        output.cpu.zero_word_count = image.get(address, model_material_field::stage_count);
        output.cpu.instance_size = image.get(address, model_material_field::instance_size);
        output.parameters.resize(image.get(address, model_material_field::constant_count));
        if (!output.parameters.empty())
        {
            std::memcpy(output.parameters.data(), image.pointer(image.get(address, model_material_field::constants)),
                output.parameters.size() * model_material_constant_field::kSize);
        }
        output.cpu.initial_parameters = output.parameters;
        output.cpu.records = image.get(address, model_material_field::stages) == 0 ? nullptr
            : image.pointer(image.get(address, model_material_field::stages));
        output.entries.resize(image.get(address, model_material_field::entry_count));
        for (u32 index = 0; index < output.entries.size(); ++index)
        {
            const u32 entry =
                image.get(address, model_material_field::entries) + index * model_material_entry_field::kSize;
            output.entries[index] = RuntimeShaderCpuEntry{
                reinterpret_cast<const char*>(image.pointer(image.get(entry, model_material_entry_field::name))),
                image.get(entry, model_material_entry_field::value)};
        }
        output.cpu.entries = output.entries;
        return &output.cpu;
    }

    RuntimeModelNode* node(u32 address)
    {
        if (address == 0)
        {
            return nullptr;
        }
        const auto found = storage.node_addresses.find(address);
        if (found != storage.node_addresses.end())
        {
            return found->second;
        }
        auto& output = storage.nodes.emplace_back();
        storage.node_addresses.emplace(address, &output);
        output.instance_offset = image.get(address, model_node_field::type_name);
        output.type_name =
            reinterpret_cast<const char*>(image.pointer(image.get(address, model_node_field::type_name)));
        output.type = image.get(address, model_node_field::type_index);
        output.copy_offset = image.get(address, model_node_field::instance_offset);
        output.descriptor = &copy_types[(image.get(address, model_node_field::descriptor) - descriptor_base) /
            model_node_type_field::kSize];
        output.bytes = image.bytes.subspan(u32(address - image.address));
        output.child = node(image.get(address, model_node_field::child));
        output.sibling = node(image.get(address, model_node_field::next));
        if (std::strcmp(output.type_name, "ndMtxArray") == 0 && image.get(address, matrix_array_node::rows) != 0)
        {
            output.matrix_records = {image.pointer(image.get(address, matrix_array_node::rows)),
                size_t(image.get(address, matrix_array_node::matrix_count)) * 8};
        }
        if (output.type == 7)
        {
            auto& joints = storage.skeletons.emplace_back(image.get(address, skeleton_node::joint_count));
            const u32 records = image.get(address, skeleton_node::joints);
            for (u32 index = 0; index < joints.size(); ++index)
            {
                const u32 record = records + index * skeleton_joint_field::kSize;
                const u16 parent = image.get(record, skeleton_joint_field::parent);
                joints[index].parent = parent == 0xFFFF ? -1 : parent;
                joints[index].local = image.get(record, skeleton_joint_field::local);
                joints[index].world = image.get(record, skeleton_joint_field::world);
            }
            output.joints = joints;
        }
        else if (output.type == 11)
        {
            output.shader_cpu = shader(image.get(address, shader_node::primary));
            if (image.get(address, shader_node::secondary) != 0)
            {
                output.secondary_shader_cpu = shader(image.get(address, shader_node::secondary));
            }
        }
        else if (output.type == 16)
        {
            auto& blendshape = storage.blendshapes.emplace_back();
            const u32 table = image.word(image.get(address, blendshape_node::targets));
            blendshape.buffers.resize(image.get(table, vertex_buffer_node::buffer_count));
            for (u32 index = 0; index < blendshape.buffers.size(); ++index)
            {
                auto& buffer = blendshape.buffers[index];
                const u32 record =
                    image.get(table, vertex_buffer_node::buffers) + index * model_vertex_buffer_field::kSize;
                for (u32 word = 0; word < 6; ++word)
                {
                    buffer.words[word] = image.word(record, word * 4);
                }
                buffer.data = buffer.words[4] == 0 ? nullptr : gpu_pointer(buffer.words[4]);
            }
            blendshape.cpu.buffers = blendshape.buffers;
            output.blendshape_cpu = &blendshape.cpu;
        }
        return &output;
    }
};

}

std::unique_ptr<RuntimeModelAssetView> view_relocated_runtime_model(
    const RuntimeRelocationImage& image, u32 directory,
    const RuntimeModelAllocation& runtime_allocation, std::span<const u8> resource_identity,
    std::span<const RuntimeModelNodeType> copy_types, u32 descriptor_base,
    const std::function<const u8*(u32)>& gpu_pointer)
{
    auto result = std::make_unique<RuntimeModelAssetView>();
    auto& storage = *result->storage;
    ViewDecoder decoder{storage, image, copy_types, descriptor_base, gpu_pointer};
    const RuntimeRelocationImage runtime{runtime_allocation.bytes, runtime_allocation.address};
    const u32 tree = runtime.word(runtime_allocation.address, 0x24);
    storage.tree.instance_size = image.word(tree);
    storage.roots.resize(image.word(tree, 0xC));
    for (u32 index = 0; index < storage.roots.size(); ++index)
    {
        storage.roots[index] = decoder.node(image.word(image.word(tree, 8) + index * 4));
    }
    storage.tree.roots = storage.roots;
    const u32 names = image.word(tree, 0x10);
    if (names != 0)
    {
        storage.names.resize(image.word(names));
        for (u32 index = 0; index < storage.names.size(); ++index)
        {
            const u32 entry = image.word(names, 4) + index * 0xC;
            const auto* name = reinterpret_cast<const char*>(image.pointer(image.word(entry)));
            const u32 value = image.word(entry, 4);
            RuntimeModelNode* named;
            if (std::strncmp(name, "shader_", 7) == 0)
            {
                named = decoder.node(tree + image.word(value));
            }
            else
            {
                named = &storage.nodes.emplace_back();
                named->instance_offset = image.word(value);
            }
            storage.names[index] = RuntimeModelNamedEntry{name, named};
        }
        storage.name_table.entries = storage.names;
        storage.tree.names = &storage.name_table;
    }
    storage.blocks.resize(image.word(directory, 4));
    for (u32 index = 0; index < storage.blocks.size(); ++index)
    {
        const u32 entry = image.word(directory) + index * 8;
        storage.blocks[index] = RuntimeModelResourceBlock{image.word(entry),
            image.word(entry, 4) == 0 ? nullptr : image.pointer(image.word(entry, 4)), image.word(entry, 4)};
    }
    result->asset = RuntimeModelAsset{storage.blocks, &storage.tree, resource_identity};
    return result;
}

// Xbox 0x00052F00. As in the dump, a full hook table has no replacement slot.
void register_runtime_model_resource_hook(AnimationResourceRegistry& registry,
    RuntimeModelResourceLoader& loader)
{
    u32 index = 0;
    AnimationResourceHook* hook = nullptr;
    do
    {
        if (registry.hooks[index].type == nullptr)
        {
            hook = &registry.hooks[index];
            break;
        }
        ++index;
    } while (index < 16);
    hook->type = "model";
    hook->relocate = [&loader](const AnimationLoadedResource& loaded)
    {
        auto& resource = loader.resources.at(loaded.cpu.data());
        relocate_runtime_model_directory(resource.image, resource.image.address,
            resource.image.address, resource.gpu, loader.relocation_types, loader.d3d);
    };
    hook->post_load = [&loader](const AnimationLoadedResource& loaded)
    {
        auto& resource = loader.resources.at(loaded.cpu.data());
        resource.runtime = postload_runtime_model_directory(resource.image, resource.image.address,
            loader.textures, loader.heap, loader.d3d);
        resource.view = view_relocated_runtime_model(resource.image, resource.image.address,
            resource.runtime, loaded.cpu, loader.copy_types, loader.relocation_types.address,
            loader.gpu_pointer);
        loader.views.assets[loaded.cpu.data()] = &resource.view->asset;
    };
    hook->unload = [&loader](const AnimationLoadedResource& loaded)
    {
        auto& resource = loader.resources.at(loaded.cpu.data());
        unload_runtime_model_directory(resource.image, resource.image.address, resource.runtime,
            loader.textures, loader.heap, loader.d3d);
        loader.views.assets.erase(loaded.cpu.data());
        resource.view.reset();
    };
}

} // namespace gbtg
