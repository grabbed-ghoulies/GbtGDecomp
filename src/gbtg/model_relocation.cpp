#include "gbtg/model_relocation.hpp"
#include <bit>

namespace gbtg
{

// Xbox 0x00159430. The declaration is copied on a miss but is not a lookup key.
i32 intern_runtime_vertex_shader(RuntimeVertexShaderCache& cache,
    const RuntimeRelocationImage& image, u32 program, u32 declaration,
    i32 key, i32 first_channel, i32 second_channel)
{
    const i32 original_count = cache.count;
    i32 words = i32(image.half(program, 2)) * 4 + 1;
    i32 index = 0;
    if (cache.count > 0)
    {
        do
        {
            const auto& entry = cache.entries[index];
            if (entry.key == key && entry.first_channel == first_channel &&
                entry.second_channel == second_channel)
            {
                i32 word = 0;
                if (words == 0)
                {
                    return index;
                }
                while (entry.program[word] == image.word(program, u32(word) * 4))
                {
                    ++word;
                    if (word >= words)
                    {
                        return index;
                    }
                }
            }
            ++index;
        } while (index < cache.count);
    }
    auto& entry = cache.entries[cache.count];
    u32 word = 0;
    for (; words != 0; --words)
    {
        entry.program[word] = image.word(program, word * 4);
        ++word;
    }
    entry.handle = 0;
    entry.key = key;
    entry.first_channel = first_channel;
    entry.second_channel = second_channel;
    i32 declaration_index = 0;
    i32 value = std::bit_cast<i32>(image.word(declaration));
    if (value != -1)
    {
        do
        {
            entry.declaration[declaration_index] = value;
            ++declaration_index;
            value = std::bit_cast<i32>(image.word(declaration, u32(declaration_index) * 4));
        } while (value != -1);
    }
    entry.declaration[declaration_index] = -1;
    ++cache.count;
    return original_count;
}

// Xbox 0x001584A0.
void relocate_runtime_vertex_shader_node(const RuntimeRelocationImage& image,
    u32 node, u32 base, RuntimeVertexShaderCache& cache)
{
    const u32 declaration = image.word(node, 0x50) + base;
    image.set_word(node, 0x50, declaration);
    const u32 program = image.word(node, 0x54) + base;
    image.set_word(node, 0x58, image.word(node, 0x58) + base);
    image.set_word(node, 0x20, image.word(node, 0x20) + base);
    image.set_word(node, 0x54, program);
    const i32 index = intern_runtime_vertex_shader(cache, image, program, declaration,
        std::bit_cast<i32>(image.word(node, 0x44)), image.word(node, 0x3C) != 0xFFFFFFFF,
        image.word(node, 0x40) != 0xFFFFFFFF);
    image.set_word(node, 0x64, u32(index));
    relocate_runtime_node_links(image, node, base);
}

// Xbox 0x001583C0.
void relocate_runtime_vertex_buffer_node(const RuntimeRelocationImage& image,
    u32 node, u32 base, u32 gpu_base, const RuntimeNodeRelocationBoundaries& boundaries)
{
    relocate_runtime_node_links(image, node, base);
    u32 index = 0;
    image.set_word(node, 0x20, image.word(node, 0x20) + base);
    if (image.word(node, 0x24) != 0)
    {
        u32 offset = 0;
        do
        {
            const u32 record = image.word(node, 0x20) + offset;
            image.set_word(record, 0x10, image.word(record, 0x10) + gpu_base);
            boundaries.register_resource(record + 4, image.word(record, 0x10));
            ++index;
            offset += 0x18;
        } while (index < image.word(node, 0x24));
    }
}

// Xbox 0x00157EA0.
void relocate_runtime_transform_node(const RuntimeRelocationImage& image,
    u32 node, u32 base, const RuntimeNodeRelocationBoundaries& boundaries)
{
    boundaries.transpose(node + 0x24, node + 0x24);
    relocate_runtime_node_links(image, node, base);
}

// Xbox 0x001573A0.
void relocate_runtime_blendshape_node(const RuntimeRelocationImage& image,
    u32 node, u32 base, u32 gpu_base, const RuntimeNodeRelocationBoundaries& boundaries)
{
    relocate_runtime_node_links(image, node, base);
    u32 shape = 0;
    image.set_word(node, 0x20, image.word(node, 0x20) + base);
    if (image.half(node, 0x24) != 0)
    {
        do
        {
            const u32 entry = image.word(node, 0x20) + shape * 4;
            image.set_word(entry, 0, image.word(entry) + base);
            const u32 table = image.word(entry);
            if (image.word(table, 0x24) != 0)
            {
                image.set_word(table, 0x20, image.word(table, 0x20) + base);
            }
            u32 vertex = 0;
            if (image.word(table, 0x24) != 0)
            {
                u32 offset = 0;
                do
                {
                    const u32 record = image.word(table, 0x20) + offset;
                    image.set_word(record, 0x10, image.word(record, 0x10) + gpu_base);
                    ++vertex;
                    offset += 0x18;
                } while (vertex < image.word(table, 0x24));
            }
            ++shape;
        } while (shape < image.half(node, 0x24));
    }
    u32 vertex = 0;
    const u32 first = image.word(image.word(node, 0x20));
    if (image.half(first, 0x24) != 0)
    {
        u32 offset = 0;
        do
        {
            const u32 record = image.word(first, 0x20) + offset;
            boundaries.register_resource(record + 4, image.word(record, 0x10));
            ++vertex;
            offset += 0x18;
        } while (vertex < image.half(first, 0x24));
    }
    if (image.half(node, 0x24) > 1)
    {
        shape = 1;
        do
        {
            const u32 table = image.word(image.word(node, 0x20) + shape * 4);
            u32 index = 0;
            if (image.byte(image.word(table, 0x20), 1) != 9)
            {
                u32 type = image.word(table, 0x20) + 1;
                do
                {
                    type += 0x18;
                    ++index;
                } while (image.byte(type) != 9);
            }
            const u32 record = image.word(table, 0x20) + index * 0x18;
            boundaries.register_resource(record + 4, image.word(record, 0x10));
            ++shape;
        } while (shape < image.half(node, 0x24));
    }
}

std::array<RuntimeNodeRelocationType, 17> runtime_model_relocation_types(
    RuntimeVertexShaderCache& cache, u32& default_pixel_shader,
    const RuntimeNodeRelocationBoundaries& d3d)
{
    auto simple = [](auto function)
    {
        return [function](const RuntimeRelocationImage& image, u32 node, u32 base, u32)
        {
            function(image, node, base);
        };
    };
    auto vertex_shader = [&cache](const RuntimeRelocationImage& image, u32 node, u32 base, u32)
    {
        relocate_runtime_vertex_shader_node(image, node, base, cache);
    };
    auto transform = [&d3d](const RuntimeRelocationImage& image, u32 node, u32 base, u32)
    {
        relocate_runtime_transform_node(image, node, base, d3d);
    };
    return {{{"ndGroup", simple(relocate_runtime_node_links)},
        {"ndPixelShaderParam", simple(relocate_runtime_pixel_shader_parameter_node)},
        {"ndPixelShader", [&default_pixel_shader, &d3d](const RuntimeRelocationImage& image,
            u32 node, u32 base, u32)
        {
            relocate_runtime_pixel_shader_node(image, node, base, default_pixel_shader, d3d);
        }},
        {"ndVertexShader", vertex_shader},
        {"ndVertexBuffer", [&d3d](const RuntimeRelocationImage& image, u32 node, u32 base, u32 gpu)
        {
            relocate_runtime_vertex_buffer_node(image, node, base, gpu, d3d);
        }},
        {"ndPushBuffer", simple(relocate_runtime_push_buffer_node)},
        {"ndBGPushBuffer", simple(relocate_runtime_background_push_buffer_node)},
        {"ndSkeleton", simple(relocate_runtime_skeleton_node)},
        {"ndVisibility", simple(relocate_runtime_node_links)},
        {"ndTransform", transform},
        {"nd2D", transform},
        {"ndShaderParam2", simple(relocate_runtime_shader_parameter_node)},
        {"ndShader2", simple(relocate_runtime_shader_node)},
        {"ndMtxArray", simple(relocate_runtime_matrix_array_node)},
        {"ndRigidSkinIdx", simple(relocate_runtime_rigid_skin_node)},
        {"ndVtxDesc", vertex_shader},
        {"ndBlendShape", [&d3d](const RuntimeRelocationImage& image, u32 node, u32 base, u32 gpu)
        {
            relocate_runtime_blendshape_node(image, node, base, gpu, d3d);
        }}}};
}

// Xbox 0x00158770. Descriptor records have an inline name and stride 0x54.
void bind_runtime_node_type(const RuntimeRelocationImage& image, u32 node,
    const RuntimeNodeRelocationTypes& types)
{
    i32 index = 0;
    if (i32(types.entries.size()) < 1)
    {
        return;
    }
    do
    {
        if (std::strcmp(types.entries[index].name.c_str(),
            reinterpret_cast<const char*>(image.pointer(image.word(node)))) == 0)
        {
            image.set_word(node, 0x10, types.address + u32(index) * 0x54);
            image.set_half(node, 4, u16(index));
            return;
        }
        ++index;
        if (i32(types.entries.size()) <= index)
        {
            return;
        }
    } while (true);
}

// Xbox 0x001588C0. This is a required vtable call, including on a lookup miss.
void dispatch_runtime_node_relocation(const RuntimeRelocationImage& image, u32 descriptor,
    u32 node, u32 base, u32 gpu_base, const RuntimeNodeRelocationTypes& types)
{
    types.entries[(descriptor - types.address) / 0x54].relocate(image, node, base, gpu_base);
}

// Xbox 0x00156600.
void relocate_runtime_node_chain(const RuntimeRelocationImage& image, u32 node,
    u32 base, u32 gpu_base, const RuntimeNodeRelocationTypes& types)
{
    for (; node != 0; node = image.word(node, 0x18))
    {
        image.set_word(node, 0, image.word(node) + base);
        bind_runtime_node_type(image, node, types);
        dispatch_runtime_node_relocation(image, image.word(node, 0x10), node, base, gpu_base, types);
        if (image.word(node, 0x14) != 0)
        {
            relocate_runtime_node_chain(image, image.word(node, 0x14), base, gpu_base, types);
        }
    }
}

// Xbox 0x00156790. FUN_00156DD0 supplies the already initialized type registry.
void relocate_runtime_model_tree(const RuntimeRelocationImage& image, u32 tree,
    u32 base, u32 gpu_base, const RuntimeNodeRelocationTypes& types)
{
    if (image.word(tree, 0x10) != 0)
    {
        const u32 names = image.word(tree, 0x10) + base;
        image.set_word(tree, 0x10, names);
        relocate_runtime_model_names(image, names, base);
    }
    if (image.word(tree, 0x14) != 0)
    {
        const u32 lists = image.word(tree, 0x14) + base;
        image.set_word(tree, 0x14, lists);
        relocate_runtime_model_pointer_lists(image, lists, base);
    }
    u32 index = 0;
    image.set_word(tree, 8, image.word(tree, 8) + base);
    if (image.word(tree, 0xC) != 0)
    {
        do
        {
            const u32 root = image.word(tree, 8) + index * 4;
            image.set_word(root, 0, image.word(root) + base);
            relocate_runtime_node_chain(image, image.word(root), base, gpu_base, types);
            ++index;
        } while (index < image.word(tree, 0xC));
    }
    if (image.word(tree, 0x28) != 0)
    {
        const u32 lists = image.word(tree, 0x28) + base;
        image.set_word(tree, 0x28, lists);
        relocate_runtime_model_pointer_lists(image, lists, base);
    }
}

// Xbox 0x00052710.
void relocate_runtime_texture_table(const RuntimeRelocationImage& image, u32 table,
    u32 base, const RuntimeGpuSegments& gpu)
{
    image.set_word(table, 4, image.word(table, 4) + base);
    u32 index = 0;
    if (image.word(table) != 0)
    {
        do
        {
            const u32 pointer = image.word(table, 4) + index * 4;
            image.set_word(pointer, 0, image.word(pointer) + base);
            const u32 texture = image.word(pointer);
            if (image.word(texture, 0x18) == 0)
            {
                image.set_word(texture, 0x14, 0);
            }
            else
            {
                image.set_word(texture, 0x14, runtime_gpu_address(gpu, image.word(texture, 0x14)));
            }
            ++index;
        } while (index < image.word(table));
    }
}

// Xbox 0x0004F500. The block arrives in ESI.
void relocate_runtime_geometry_block(const RuntimeRelocationImage& image, u32 block,
    u32 base, const RuntimeGpuSegments& gpu, const RuntimeNodeRelocationBoundaries& boundaries)
{
    if (image.word(block, 8) != 0)
    {
        image.set_word(block, 8, image.word(block, 8) + base);
    }
    if (image.word(block, 0xC) != 0)
    {
        image.set_word(block, 0xC, image.word(block, 0xC) + base);
    }
    if (image.word(block, 0x3C) != 0)
    {
        image.set_word(block, 0x3C, image.word(block, 0x3C) + base);
        u32 index = 0;
        if (image.word(block, 0x1C) != 0)
        {
            u32 offset = 0;
            do
            {
                const u32 stream = image.word(block, 0x3C) + offset;
                const u32 gpu_offset = image.word(stream, 0xC);
                if (gpu_offset != 0)
                {
                    image.set_word(stream, 0xC, runtime_gpu_address(gpu, gpu_offset));
                    boundaries.set_vertex_buffer_header(image.word(stream, 8), 8, 0, 0, stream + 0x10, 0);
                    boundaries.register_resource(stream + 0x10, image.word(stream, 0xC));
                }
                ++index;
                offset += 0x1C;
            } while (index < image.word(block, 0x1C));
        }
    }
    if (image.word(block, 0x40) != 0)
    {
        image.set_word(block, 0x40, image.word(block, 0x40) + base);
    }
}

// Xbox 0x00158680. The assembler reads the original immutable XBE strings at
// these addresses; the D3D boundary owns assembly buffers and shader handles.
void relocate_runtime_pixel_shader_node(const RuntimeRelocationImage& image,
    u32 node, u32 base, u32& default_pixel_shader,
    const RuntimeNodeRelocationBoundaries& boundaries)
{
    u32 buffer;
    boundaries.assemble_shader(0x315A98, 0x32E4B8, 0x12, 4, 0, buffer, 0, 0, 0, 0, 0);
    boundaries.create_pixel_shader(boundaries.assembled_shader_data(buffer), default_pixel_shader);
    relocate_runtime_node_links(image, node, base);
}

// Xbox 0x00054490.
void relocate_runtime_physics_simple(const RuntimeRelocationImage& image, u32 record, u32 base)
{
    if (image.word(record, 0x14) != 0)
    {
        image.set_word(record, 0x14, image.word(record, 0x14) + base);
    }
}

// Xbox 0x000536F0.
void relocate_runtime_physics_pairs(const RuntimeRelocationImage& image, u32 record, u32 base)
{
    i32 index = 0;
    if (image.half(record, 0x20) != 0)
    {
        u32 pointer = record + 0x30;
        do
        {
            if (image.word(pointer) != 0)
            {
                image.set_word(pointer, 0, image.word(pointer) + base);
            }
            ++index;
            pointer += 8;
        } while (index < i32(image.half(record, 0x20)));
    }
}

// Xbox 0x00055620. The three records at 0x0042380C have stride 0x14.
void relocate_runtime_physics_block(const RuntimeRelocationImage& image, u32 block, u32 base)
{
    using Relocator = void (*)(const RuntimeRelocationImage&, u32, u32);
    static constexpr std::array<Relocator, 3> callbacks{
        relocate_runtime_physics_simple, relocate_runtime_physics_pairs, relocate_runtime_physics_simple};
    image.set_word(block, 0xC, image.word(block, 0xC) + base);
    u32 record = block + 0x10;
    if (std::bit_cast<i32>(image.word(block)) > 0)
    {
        i32 index = 0;
        do
        {
            const auto callback = callbacks[image.byte(record)];
            if (callback != nullptr)
            {
                callback(image, record, base);
            }
            ++index;
            record += image.word(record, 8);
        } while (index < std::bit_cast<i32>(image.word(block)));
    }
}

// Xbox 0x00052910. The tree's pointer base is its own relocated address,
// whereas other directory blocks use the CPU allocation's base.
void relocate_runtime_model_directory(const RuntimeRelocationImage& image,
    u32 directory, u32 base, const RuntimeGpuSegments* gpu,
    const RuntimeNodeRelocationTypes& types, const RuntimeNodeRelocationBoundaries& boundaries)
{
    image.set_word(directory, 0, image.word(directory) + base);
    u32 index = 0;
    if (image.word(directory, 4) != 0)
    {
        do
        {
            u32 entry = image.word(directory) + index * 8;
            u32 block;
            switch (image.word(entry))
            {
            case 0:
                image.set_word(entry, 4, image.word(entry, 4) + base);
                block = image.word(entry, 4);
                image.set_word(block, 0, image.word(block) + base);
                if (gpu != nullptr)
                {
                    image.set_word(block, 4, runtime_gpu_address(*gpu, image.word(block, 4)));
                }
                relocate_runtime_model_tree(image, image.word(block), image.word(block),
                    image.word(block, 4), types);
                break;
            case 2:
            case 3:
            case 4:
            case 5:
            case 8:
            case 9:
            case 10:
            case 0xF:
            case 0x14:
            case 0x15:
                image.set_word(entry, 4, image.word(entry, 4) + base);
                break;
            case 6:
                image.set_word(entry, 4, image.word(entry, 4) + base);
                relocate_runtime_physics_block(image, image.word(entry, 4), base);
                break;
            case 7:
                image.set_word(entry, 4, image.word(entry, 4) + base);
                relocate_runtime_texture_table(image, image.word(entry, 4), base, *gpu);
                break;
            case 0xB:
            case 0x17:
                image.set_word(entry, 4, image.word(entry, 4) + base);
                block = image.word(entry, 4);
                if (image.word(block, 0xC) != 0)
                {
                    image.set_word(block, 0xC, image.word(block, 0xC) + base);
                }
                break;
            case 0xC:
            case 0xD:
                image.set_word(entry, 4, image.word(entry, 4) + base);
                block = image.word(entry, 4);
                if (image.word(block, 4) != 0)
                {
                    image.set_word(block, 4, image.word(block, 4) + base);
                }
                relocate_runtime_texture_table(image, block + 8, base, *gpu);
                break;
            case 0xE:
                image.set_word(entry, 4, image.word(entry, 4) + base);
                relocate_runtime_model_type14(image, image.word(entry, 4), base);
                break;
            case 0x10:
                image.set_word(entry, 4, image.word(entry, 4) + base);
                block = image.word(entry, 4);
                if (image.word(block, 4) != 0)
                {
                    image.set_word(block, 4, image.word(block, 4) + base);
                }
                if (image.word(block, 0xC) != 0)
                {
                    image.set_word(block, 0xC, image.word(block, 0xC) + base);
                }
                if (image.word(block, 8) != 0)
                {
                    image.set_word(block, 8, image.word(block, 8) + base);
                }
                break;
            case 0x11:
                image.set_word(entry, 4, image.word(entry, 4) + base);
                relocate_runtime_model_type17(image, image.word(entry, 4), base);
                break;
            case 0x12:
                image.set_word(entry, 4, image.word(entry, 4) + base);
                relocate_runtime_geometry_block(image, image.word(entry, 4), base, *gpu, boundaries);
                break;
            case 0x13:
                image.set_word(entry, 4, image.word(entry, 4) + base);
                block = image.word(entry, 4);
                if (image.word(block, 4) != 0)
                {
                    image.set_word(block, 4, image.word(block, 4) + base);
                }
                if (image.word(block) != 0)
                {
                    image.set_word(block, 0, image.word(block) + base);
                }
                break;
            case 0x16:
                image.set_word(entry, 4, image.word(entry, 4) + base);
                relocate_runtime_model_type22(image, image.word(entry, 4), base);
                break;
            case 0x18:
                image.set_word(entry, 4, image.word(entry, 4) + base);
                relocate_runtime_model_type24(image, image.word(entry, 4), base);
                break;
            case 0x19:
            case 0x1A:
                image.set_word(entry, 4, image.word(entry, 4) + base);
                block = image.word(entry, 4);
                if (image.word(block, 4) != 0)
                {
                    image.set_word(block, 4, image.word(block, 4) + base);
                }
                break;
            }
            ++index;
        } while (index < image.word(directory, 4));
    }
}

// Xbox 0x00155DA0.
void postload_runtime_matrix_array(const RuntimeRelocationImage& image, u32 node,
    const RuntimeModelHeapBoundaries& heap)
{
    if (image.word(node, 0x30) != 0 && image.word(node, 0x2C) != 0)
    {
        const auto allocation = heap.allocate(image.word(node, 0x30) << 6);
        u32 source = image.word(node, 0x2C);
        u32 destination = 0;
        for (u32 count = (image.word(node, 0x30) & 0x3FFFFFF) << 4; count != 0; --count)
        {
            const u32 value = image.word(source);
            std::memcpy(allocation.bytes.data() + destination, &value, 4);
            source += 4;
            destination += 4;
        }
        image.set_word(node, 0x2C, allocation.address);
        image.set_byte(node, 0x29, 1);
        return;
    }
    image.set_byte(node, 0x29, 0);
}

// Xbox 0x00155DF0.
void unload_runtime_matrix_array(const RuntimeRelocationImage& image, u32 node,
    const RuntimeModelHeapBoundaries& heap)
{
    if (image.word(node, 0x2C) != 0 && image.byte(node, 0x29) != 0)
    {
        heap.free(image.word(node, 0x2C));
        image.set_word(node, 0x2C, node + image.half(node, 0x2A));
    }
}

// Xbox 0x001581D0. FUN_00155D00 selects the physical allocator callback.
void postload_runtime_push_buffer(const RuntimeRelocationImage& image, u32 node,
    const RuntimeModelHeapBoundaries& heap,
    const std::function<void(u32, u32, u32, u32)>& set_push_buffer_header,
    const RuntimeNodeRelocationBoundaries& d3d)
{
    if (image.word(node, 0x24) != 0)
    {
        const auto allocation = heap.allocate_physical(image.word(node, 0x2C), 0x80,
            ".\\nodes\\geometry\\pushbuffer.c", 0xF4);
        const u32 size = image.word(node, 0x2C);
        u32 source = image.word(node, 0x28);
        u32 destination = 0;
        for (u32 count = size >> 2; count != 0; --count)
        {
            const u32 value = image.word(source);
            std::memcpy(allocation.bytes.data() + destination, &value, 4);
            source += 4;
            destination += 4;
        }
        for (u32 count = size & 3; count != 0; --count)
        {
            allocation.bytes[destination] = image.byte(source);
            ++source;
            ++destination;
        }
        image.set_word(node, 0x28, allocation.address);
        set_push_buffer_header(image.word(node, 0x2C), 0, image.word(node, 0x24), 0);
        d3d.register_resource(image.word(node, 0x24), image.word(node, 0x28));
    }
}

// Xbox 0x0015B360.
void postload_runtime_pixel_shader(const RuntimeRelocationImage& image, u32 shader,
    const RuntimeNodeRelocationBoundaries& d3d)
{
    u32 handle = image.word(shader, 0xC);
    d3d.create_pixel_shader(image.word(shader, 4), handle);
    image.set_word(shader, 0xC, handle);
}

// Xbox 0x0015B640.
void unload_runtime_pixel_shader(const RuntimeRelocationImage& image, u32 shader,
    const RuntimeNodeRelocationBoundaries& d3d)
{
    if (image.word(shader, 0xC) != 0)
    {
        d3d.delete_pixel_shader(image.word(shader, 0xC));
        image.set_word(shader, 0xC, 0);
    }
}

// Xbox 0x00157CB0.
void postload_runtime_shader_node(const RuntimeRelocationImage& image, u32 node,
    const RuntimeNodeRelocationBoundaries& d3d)
{
    postload_runtime_pixel_shader(image, image.word(node, 0x20), d3d);
    if (image.word(node, 0x24) != 0)
    {
        postload_runtime_pixel_shader(image, image.word(node, 0x24), d3d);
    }
}

// Xbox 0x00157D00.
void unload_runtime_shader_node(const RuntimeRelocationImage& image, u32 node,
    const RuntimeNodeRelocationBoundaries& d3d)
{
    unload_runtime_pixel_shader(image, image.word(node, 0x20), d3d);
    if (image.word(node, 0x24) != 0)
    {
        unload_runtime_pixel_shader(image, image.word(node, 0x24), d3d);
    }
}

// Xbox 0x00156660. The comparisons include the terminating NUL.
void postload_runtime_node_chain(const RuntimeRelocationImage& image, u32 node,
    const RuntimeModelHeapBoundaries& heap, const RuntimeNodeRelocationBoundaries& d3d)
{
    do
    {
        if (node == 0)
        {
            return;
        }
        const auto* name = image.pointer(image.word(node));
        if (std::memcmp(name, "ndVertexBuffer", 15) == 0)
        {
            runtime_model_noop();
        }
        else if (std::memcmp(name, "ndPushBuffer", 13) == 0)
        {
            postload_runtime_push_buffer(image, node, heap, d3d.set_push_buffer_header, d3d);
        }
        else if (std::memcmp(name, "ndShader2", 10) == 0)
        {
            postload_runtime_shader_node(image, node, d3d);
        }
        else if (std::memcmp(name, "ndBlendShape", 13) == 0)
        {
            runtime_model_noop();
        }
        else if (std::memcmp(name, "ndMtxArray", 11) == 0)
        {
            postload_runtime_matrix_array(image, node, heap);
        }
        if (image.word(node, 0x14) != 0)
        {
            postload_runtime_node_chain(image, image.word(node, 0x14), heap, d3d);
        }
        node = image.word(node, 0x18);
    } while (true);
}

// Xbox 0x00156720.
void unload_runtime_node_chain(const RuntimeRelocationImage& image, u32 node,
    const RuntimeModelHeapBoundaries& heap, const RuntimeNodeRelocationBoundaries& d3d)
{
    do
    {
        if (node == 0)
        {
            return;
        }
        const auto* name = image.pointer(image.word(node));
        if (std::memcmp(name, "ndShader2", 10) == 0)
        {
            unload_runtime_shader_node(image, node, d3d);
        }
        else if (std::memcmp(name, "ndMtxArray", 11) == 0)
        {
            unload_runtime_matrix_array(image, node, heap);
        }
        if (image.word(node, 0x14) != 0)
        {
            unload_runtime_node_chain(image, image.word(node, 0x14), heap, d3d);
        }
        node = image.word(node, 0x18);
    } while (true);
}

// Xbox 0x00156840.
void postload_runtime_model_tree(const RuntimeRelocationImage& image, u32 tree,
    const RuntimeModelHeapBoundaries& heap, const RuntimeNodeRelocationBoundaries& d3d)
{
    u32 index = 0;
    if (image.word(tree, 0xC) != 0)
    {
        do
        {
            postload_runtime_node_chain(image, image.word(image.word(tree, 8) + index * 4), heap, d3d);
            ++index;
        } while (index < image.word(tree, 0xC));
    }
}

// Xbox 0x00156870.
void unload_runtime_model_tree(const RuntimeRelocationImage& image, u32 tree,
    const RuntimeModelHeapBoundaries& heap, const RuntimeNodeRelocationBoundaries& d3d)
{
    u32 index = 0;
    if (image.word(tree, 0xC) != 0)
    {
        do
        {
            unload_runtime_node_chain(image, image.word(image.word(tree, 8) + index * 4), heap, d3d);
            ++index;
        } while (index < image.word(tree, 0xC));
    }
}

// Xbox 0x00051F20. Offset arrives in EBX in the original.
u32 runtime_gpu_address(const RuntimeGpuSegments& gpu, u32 offset)
{
    u32 index = 0;
    if (gpu.segments.size() != 0)
    {
        u32 start = 0;
        do
        {
            const u32 size = gpu.segments[index].size;
            if (offset < start + size)
            {
                return gpu.segments[index].address - start + offset;
            }
            ++index;
            start += size;
        } while (index < gpu.segments.size());
    }
    return 0;
}

// Xbox 0x001568A0.
void relocate_runtime_model_names(const RuntimeRelocationImage& image, u32 names, u32 base)
{
    image.set_word(names, 4, image.word(names, 4) + base);
    u32 index = 0;
    if (image.word(names) != 0)
    {
        u32 offset = 0;
        do
        {
            const u32 entry = image.word(names, 4) + offset;
            image.set_word(entry, 0, image.word(entry) + base);
            image.set_word(entry, 4, image.word(entry, 4) + base);
            ++index;
            offset += 0xC;
        } while (index < image.word(names));
    }
}

// Xbox 0x00158B50.
void relocate_runtime_model_pointer_lists(const RuntimeRelocationImage& image, u32 lists, u32 base)
{
    u32 index = 0;
    image.set_word(lists, 4, image.word(lists, 4) + base);
    if (image.word(lists) != 0)
    {
        u32 offset = 0;
        do
        {
            const u32 entry = image.word(lists, 4) + offset;
            image.set_word(entry, 0, image.word(entry) + base);
            image.set_word(entry, 8, image.word(entry, 8) + base);
            u32 item = 0;
            if (image.word(entry, 4) != 0)
            {
                do
                {
                    const u32 pointer = image.word(entry, 8) + item * 4;
                    image.set_word(pointer, 0, image.word(pointer) + base);
                    ++item;
                } while (item < image.word(entry, 4));
            }
            ++index;
            offset += 0xC;
        } while (index < image.word(lists));
    }
}

// Xbox 0x00053320.
void relocate_runtime_model_type14(const RuntimeRelocationImage& image, u32 block, u32 base)
{
    if (image.word(block, 8) != 0)
    {
        image.set_word(block, 8, image.word(block, 8) + base);
        image.set_word(block, 0xC, image.word(block, 0xC) + base);
        if (image.word(block, 0x14) != 0)
        {
            image.set_word(block, 0x44, image.word(block, 0x44) + base);
            u32 index = 0;
            if (image.word(block, 0x14) != 0)
            {
                do
                {
                    const u32 entry = image.word(block, 0x44) + index * 8;
                    if (image.word(entry) != 0)
                    {
                        image.set_word(entry, 4, image.word(entry, 4) + base);
                    }
                    ++index;
                } while (index < image.word(block, 0x14));
            }
        }
    }
}

// Xbox 0x000531D0.
void relocate_runtime_model_type17(const RuntimeRelocationImage& image, u32 block, u32 base)
{
    if (image.word(block, 8) != 0)
    {
        image.set_word(block, 8, image.word(block, 8) + base);
        u32 index = 0;
        if (image.byte(block, 1) != 0)
        {
            do
            {
                const u32 entry = image.word(block, 8) + 4 + index * 8;
                image.set_word(entry, 0, image.word(entry) + base);
                ++index;
            } while (index < image.byte(block, 1));
        }
    }
    if (image.word(block, 0xC) != 0)
    {
        image.set_word(block, 0xC, image.word(block, 0xC) + base);
        if (image.byte(block, 1) != 0)
        {
            u32 index = 0;
            do
            {
                const u32 entry = image.word(block, 0xC) + 4 + index * 8;
                image.set_word(entry, 0, image.word(entry) + base);
                ++index;
            } while (index < image.byte(block, 1));
        }
    }
    if (image.word(block, 0x10) != 0)
    {
        image.set_word(block, 0x10, image.word(block, 0x10) + base);
        u32 index = 0;
        if (image.byte(block, 1) != 0)
        {
            do
            {
                const u32 entry = image.word(block, 0x10) + index * 4;
                const u32 pointer = image.word(entry);
                if (pointer != 0)
                {
                    image.set_word(entry, 0, pointer + base);
                }
                ++index;
            } while (index < image.byte(block, 1));
        }
    }
}

// Xbox 0x00051F60.
void relocate_runtime_model_type22(const RuntimeRelocationImage& image, u32 block, u32 base)
{
    if (image.word(block, 4) != 0)
    {
        image.set_word(block, 4, image.word(block, 4) + base);
        u32 index = 0;
        if (image.word(block) != 0)
        {
            do
            {
                const u32 entry = image.word(block, 4) + index * 4;
                image.set_word(entry, 0, image.word(entry) + base);
                const u32 resource = image.word(entry);
                image.set_word(resource, 0x4C, image.word(resource, 0x4C) + base);
                u32 item = 0;
                if (image.word(resource) != 0)
                {
                    do
                    {
                        const u32 pointer = image.word(resource, 0x4C) + item * 4;
                        image.set_word(pointer, 0, image.word(pointer) + base);
                        ++item;
                    } while (item < image.word(resource));
                }
                ++index;
            } while (index < image.word(block));
        }
    }
    if (image.word(block, 0xC) != 0)
    {
        image.set_word(block, 0xC, image.word(block, 0xC) + base);
        if (image.word(block, 8) != 0)
        {
            u32 index = 0;
            do
            {
                const u32 pointer = image.word(block, 0xC) + index * 4;
                image.set_word(pointer, 0, image.word(pointer) + base);
                ++index;
            } while (index < image.word(block, 8));
        }
    }
    if (image.word(block, 0x10) != 0)
    {
        image.set_word(block, 0x10, image.word(block, 0x10) + base);
    }
    if (image.word(block, 0x14) != 0)
    {
        image.set_word(block, 0x14, image.word(block, 0x14) + base);
    }
    if (image.word(block, 0x18) != 0)
    {
        image.set_word(block, 0x18, image.word(block, 0x18) + base);
    }
    if (image.word(block, 0x1C) != 0)
    {
        image.set_word(block, 0x1C, image.word(block, 0x1C) + base);
    }
    if (image.word(block, 0x20) != 0)
    {
        image.set_word(block, 0x20, image.word(block, 0x20) + base);
    }
    if (image.word(block, 0x24) != 0)
    {
        image.set_word(block, 0x24, image.word(block, 0x24) + base);
    }
}

// Xbox 0x00053270.
void relocate_runtime_model_type24(const RuntimeRelocationImage& image, u32 block, u32 base)
{
    image.set_word(block, 8, image.word(block, 8) + base);
    image.set_word(block, 0x1C, image.word(block, 0x1C) + base);
    u32 index = 0;
    if (image.byte(block) != 0)
    {
        do
        {
            const u32 entry = image.word(block, 8) + 4 + index * 8;
            image.set_word(entry, 0, image.word(entry) + base);
            ++index;
        } while (index < image.byte(block));
    }
}

// Xbox 0x0015ACB0.
void relocate_runtime_shader_parameters(const RuntimeRelocationImage& image, u32 shader, u32 base)
{
    if (image.word(shader, 8) != 0)
    {
        image.set_word(shader, 8, image.word(shader, 8) + base);
    }
    if (image.word(shader) != 0)
    {
        image.set_word(shader, 0, image.word(shader) + base);
    }
    if (image.word(shader, 4) != 0)
    {
        image.set_word(shader, 4, image.word(shader, 4) + base);
    }
    if (image.word(shader, 0x20) != 0)
    {
        image.set_word(shader, 0x20, image.word(shader, 0x20) + base);
    }
    if (image.word(shader, 0x24) != 0)
    {
        image.set_word(shader, 0x24, image.word(shader, 0x24) + base);
        u32 index = 0;
        if (image.word(shader, 0x28) != 0)
        {
            u32 offset = 0;
            do
            {
                const u32 entry = image.word(shader, 0x24) + offset;
                image.set_word(entry, 0, image.word(entry) + base);
                ++index;
                offset += 0x10;
            } while (index < image.word(shader, 0x28));
        }
    }
}

// Xbox 0x0015B340.
void relocate_runtime_pixel_shader(const RuntimeRelocationImage& image, u32 shader, u32 base)
{
    image.set_word(shader, 4, image.word(shader, 4) + base);
}

// Xbox 0x00158910.
void relocate_runtime_node_links(const RuntimeRelocationImage& image, u32 node, u32 base)
{
    if (image.word(node, 0x14) != 0)
    {
        image.set_word(node, 0x14, image.word(node, 0x14) + base);
    }
    if (image.word(node, 0x18) != 0)
    {
        image.set_word(node, 0x18, image.word(node, 0x18) + base);
    }
    if (image.word(node, 0x1C) != 0)
    {
        image.set_word(node, 0x1C, image.word(node, 0x1C) + base);
    }
}

// Xbox 0x00158720 (the original thunk contains the same three branches).
void relocate_runtime_pixel_shader_parameter_node(const RuntimeRelocationImage& image, u32 node, u32 base)
{
    relocate_runtime_node_links(image, node, base);
}

// Xbox 0x00158160.
void relocate_runtime_push_buffer_node(const RuntimeRelocationImage& image, u32 node, u32 base)
{
    relocate_runtime_node_links(image, node, base);
    if (image.word(node, 0x24) != 0)
    {
        image.set_word(node, 0x24, image.word(node, 0x24) + base);
        image.set_word(node, 0x28, image.word(node, 0x28) + base);
        return;
    }
    image.set_word(node, 0x30, image.word(node, 0x30) + base);
    image.set_word(node, 0x38, image.word(node, 0x38) + base);
    u32 index = 0;
    image.set_word(node, 0x34, image.word(node, 0x34) + base);
    if (image.word(node, 0x20) != 0)
    {
        do
        {
            const u32 entry = image.word(node, 0x30) + index * 4;
            image.set_word(entry, 0, image.word(entry) + base);
            ++index;
        } while (index < image.word(node, 0x20));
    }
}

// Xbox 0x001580A0.
void relocate_runtime_background_push_buffer_node(const RuntimeRelocationImage& image, u32 node, u32 base)
{
    image.set_word(node, 0x44, image.word(node, 0x44) + base);
    image.set_word(node, 0x40, image.word(node, 0x40) + base);
    relocate_runtime_node_links(image, node, base);
    if (image.word(node, 0x24) != 0)
    {
        image.set_word(node, 0x24, image.word(node, 0x24) + base);
        image.set_word(node, 0x28, image.word(node, 0x28) + base);
        return;
    }
    image.set_word(node, 0x30, image.word(node, 0x30) + base);
    image.set_word(node, 0x38, image.word(node, 0x38) + base);
    u32 index = 0;
    image.set_word(node, 0x34, image.word(node, 0x34) + base);
    if (image.word(node, 0x20) != 0)
    {
        do
        {
            const u32 entry = image.word(node, 0x30) + index * 4;
            image.set_word(entry, 0, image.word(entry) + base);
            ++index;
        } while (index < image.word(node, 0x20));
    }
}

// Xbox 0x001569A0.
void relocate_runtime_skeleton_node(const RuntimeRelocationImage& image, u32 node, u32 base)
{
    relocate_runtime_node_links(image, node, base);
    image.set_word(node, 0x24, image.word(node, 0x24) + base);
}

// Xbox 0x001572D0.
void relocate_runtime_shader_parameter_node(const RuntimeRelocationImage& image, u32 node, u32 base)
{
    u32 shader = image.word(node, 0x20) + base;
    image.set_word(node, 0x20, shader);
    relocate_runtime_shader_parameters(image, shader, base);
    if (image.word(node, 0x24) != 0)
    {
        shader = image.word(node, 0x24) + base;
        image.set_word(node, 0x24, shader);
        relocate_runtime_shader_parameters(image, shader, base);
    }
    relocate_runtime_node_links(image, node, base);
}

// Xbox 0x00157C60.
void relocate_runtime_shader_node(const RuntimeRelocationImage& image, u32 node, u32 base)
{
    u32 shader = image.word(node, 0x20) + base;
    image.set_word(node, 0x20, shader);
    relocate_runtime_pixel_shader(image, shader, base);
    if (image.word(node, 0x24) != 0)
    {
        shader = image.word(node, 0x24) + base;
        image.set_word(node, 0x24, shader);
        relocate_runtime_pixel_shader(image, shader, base);
    }
    relocate_runtime_node_links(image, node, base);
}

// Xbox 0x001628B0. The retail function has no body beyond RET.
void runtime_model_noop()
{
}

// Xbox 0x00155D50.
void relocate_runtime_matrix_array_node(const RuntimeRelocationImage& image, u32 node, u32 base)
{
    relocate_runtime_node_links(image, node, base);
    const u32 matrices = image.word(node, 0x2C) + base;
    image.set_word(node, 0x20, image.word(node, 0x20) + base);
    image.set_word(node, 0x2C, matrices);
    if (matrices != 0 && image.word(node, 0x30) != 0)
    {
        const i32 offset = std::bit_cast<i32>(matrices - node);
        if (offset < 0 || offset > 0xFFFF)
        {
            runtime_model_noop();
        }
        image.set_half(node, 0x2A, u16(offset));
    }
}

// Xbox 0x00157C40.
void relocate_runtime_rigid_skin_node(const RuntimeRelocationImage& image, u32 node, u32 base)
{
    image.set_word(node, 0x20, image.word(node, 0x20) + base);
    relocate_runtime_node_links(image, node, base);
}

} // namespace gbtg
