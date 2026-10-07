#include "gbtg/model_views.hpp"
#include <bit>
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
        output.cpu.zero_word_count = image.word(address, 0xC);
        output.cpu.instance_size = image.word(address, 0x44);
        output.parameters.resize(image.word(address, 0x10));
        if (!output.parameters.empty())
        {
            std::memcpy(output.parameters.data(), image.pointer(image.word(address, 4)),
                output.parameters.size() * 0x48);
        }
        output.cpu.initial_parameters = output.parameters;
        output.entries.resize(image.word(address, 0x28));
        for (u32 index = 0; index < output.entries.size(); ++index)
        {
            const u32 entry = image.word(address, 0x24) + index * 0x10;
            output.entries[index] = RuntimeShaderCpuEntry{
                reinterpret_cast<const char*>(image.pointer(image.word(entry))),
                std::bit_cast<i32>(image.word(entry, 8))};
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
        output.instance_offset = image.word(address);
        output.type_name = reinterpret_cast<const char*>(image.pointer(image.word(address)));
        output.type = image.half(address, 4);
        output.copy_offset = image.word(address, 0xC);
        output.descriptor = &copy_types[(image.word(address, 0x10) - descriptor_base) / 0x54];
        output.bytes = image.bytes.subspan(u32(address - image.address));
        output.child = node(image.word(address, 0x14));
        output.sibling = node(image.word(address, 0x18));
        if (output.type == 7)
        {
            auto& joints = storage.skeletons.emplace_back(image.word(address, 0x20));
            const u32 records = image.word(address, 0x24);
            for (u32 index = 0; index < joints.size(); ++index)
            {
                const u32 record = records + index * 0x20;
                const u16 parent = image.half(record);
                joints[index].parent = parent == 0xFFFF ? -1 : parent;
                joints[index].local = Vec3{
                    std::bit_cast<float>(image.word(record, 4)),
                    std::bit_cast<float>(image.word(record, 8)),
                    std::bit_cast<float>(image.word(record, 0xC))};
                joints[index].world = Vec3{
                    std::bit_cast<float>(image.word(record, 0x10)),
                    std::bit_cast<float>(image.word(record, 0x14)),
                    std::bit_cast<float>(image.word(record, 0x18))};
            }
            output.joints = joints;
        }
        else if (output.type == 11)
        {
            output.shader_cpu = shader(image.word(address, 0x20));
            if (image.word(address, 0x24) != 0)
            {
                output.secondary_shader_cpu = shader(image.word(address, 0x24));
            }
        }
        else if (output.type == 16)
        {
            auto& blendshape = storage.blendshapes.emplace_back();
            const u32 table = image.word(image.word(address, 0x20));
            blendshape.buffers.resize(image.word(table, 0x24));
            for (u32 index = 0; index < blendshape.buffers.size(); ++index)
            {
                auto& buffer = blendshape.buffers[index];
                const u32 record = image.word(table, 0x20) + index * 0x18;
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
            image.word(entry, 4) == 0 ? nullptr : image.pointer(image.word(entry, 4))};
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
