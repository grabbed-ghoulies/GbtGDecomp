#include "gbtg/model_relocation.h"
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
    entry.variant_count = 0;
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
    const u32 declaration = image.get(node, vertex_shader_node::declaration) + base;
    image.set(node, vertex_shader_node::declaration, declaration);
    const u32 program = image.get(node, vertex_shader_node::program) + base;
    image.set(node, vertex_shader_node::strides, image.get(node, vertex_shader_node::strides) + base);
    image.set(node, vertex_shader_node::streams, image.get(node, vertex_shader_node::streams) + base);
    image.set(node, vertex_shader_node::program, program);
    const i32 index = intern_runtime_vertex_shader(cache, image, program, declaration,
        image.get(node, vertex_shader_node::variant), image.get(node, vertex_shader_node::first_channel) != 0xFFFFFFFF,
        image.get(node, vertex_shader_node::second_channel) != 0xFFFFFFFF);
    image.set(node, vertex_shader_node::shader, u32(index));
    relocate_runtime_node_links(image, node, base);
}

// Xbox 0x001583C0.
void relocate_runtime_vertex_buffer_node(const RuntimeRelocationImage& image,
    u32 node, u32 base, u32 gpu_base, const RuntimeNodeRelocationBoundaries& boundaries)
{
    relocate_runtime_node_links(image, node, base);
    u32 index = 0;
    image.set(node, vertex_buffer_node::buffers, image.get(node, vertex_buffer_node::buffers) + base);
    if (image.get(node, vertex_buffer_node::buffer_count) != 0)
    {
        u32 offset = 0;
        do
        {
            const u32 record = image.get(node, vertex_buffer_node::buffers) + offset;
            image.set(record, model_vertex_buffer_field::data,
                image.get(record, model_vertex_buffer_field::data) + gpu_base);
            boundaries.register_resource(record + model_vertex_buffer_field::header_offset,
                image.get(record, model_vertex_buffer_field::data));
            ++index;
            offset += model_vertex_buffer_field::kSize;
        } while (index < image.get(node, vertex_buffer_node::buffer_count));
    }
}

// Xbox 0x00157EA0.
void relocate_runtime_transform_node(const RuntimeRelocationImage& image,
    u32 node, u32 base, const RuntimeNodeRelocationBoundaries& boundaries)
{
    boundaries.transpose(node + transform_node::matrix_offset, node + transform_node::matrix_offset);
    relocate_runtime_node_links(image, node, base);
}

// Xbox 0x001573A0.
void relocate_runtime_blendshape_node(const RuntimeRelocationImage& image,
    u32 node, u32 base, u32 gpu_base, const RuntimeNodeRelocationBoundaries& boundaries)
{
    relocate_runtime_node_links(image, node, base);
    u32 shape = 0;
    image.set(node, blendshape_node::targets, image.get(node, blendshape_node::targets) + base);
    if (image.get(node, blendshape_node::target_count) != 0)
    {
        do
        {
            const u32 entry = image.get(node, blendshape_node::targets) + shape * 4;
            image.set_word(entry, 0, image.word(entry) + base);
            const u32 table = image.word(entry);
            if (image.get(table, vertex_buffer_node::buffer_count) != 0)
            {
                image.set(table, vertex_buffer_node::buffers, image.get(table, vertex_buffer_node::buffers) + base);
            }
            u32 vertex = 0;
            if (image.get(table, vertex_buffer_node::buffer_count) != 0)
            {
                u32 offset = 0;
                do
                {
                    const u32 record = image.get(table, vertex_buffer_node::buffers) + offset;
                    image.set(record, model_vertex_buffer_field::data,
                        image.get(record, model_vertex_buffer_field::data) + gpu_base);
                    ++vertex;
                    offset += model_vertex_buffer_field::kSize;
                } while (vertex < image.get(table, vertex_buffer_node::buffer_count));
            }
            ++shape;
        } while (shape < image.get(node, blendshape_node::target_count));
    }
    u32 vertex = 0;
    const u32 first = image.word(image.get(node, blendshape_node::targets));
    if (u16(image.get(first, vertex_buffer_node::buffer_count)) != 0)
    {
        u32 offset = 0;
        do
        {
            const u32 record = image.get(first, vertex_buffer_node::buffers) + offset;
            boundaries.register_resource(record + model_vertex_buffer_field::header_offset,
                image.get(record, model_vertex_buffer_field::data));
            ++vertex;
            offset += model_vertex_buffer_field::kSize;
        } while (vertex < u16(image.get(first, vertex_buffer_node::buffer_count)));
    }
    if (image.get(node, blendshape_node::target_count) > 1)
    {
        shape = 1;
        do
        {
            const u32 table = image.word(image.get(node, blendshape_node::targets) + shape * 4);
            u32 index = 0;
            if (image.byte(image.get(table, vertex_buffer_node::buffers), 1) != 9)
            {
                u32 type = image.get(table, vertex_buffer_node::buffers) + 1;
                do
                {
                    type += 0x18;
                    ++index;
                } while (image.byte(type) != 9);
            }
            const u32 record = image.get(table, vertex_buffer_node::buffers) + index * model_vertex_buffer_field::kSize;
            boundaries.register_resource(record + model_vertex_buffer_field::header_offset,
                image.get(record, model_vertex_buffer_field::data));
            ++shape;
        } while (shape < image.get(node, blendshape_node::target_count));
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
            image.set(node, model_node_field::descriptor, types.address + u32(index) * model_node_type_field::kSize);
            image.set(node, model_node_field::type_index, u16(index));
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
    types.entries[(descriptor - types.address) / model_node_type_field::kSize].relocate(image, node, base, gpu_base);
}

// Xbox 0x00156600.
void relocate_runtime_node_chain(const RuntimeRelocationImage& image, u32 node,
    u32 base, u32 gpu_base, const RuntimeNodeRelocationTypes& types)
{
    for (; node != 0; node = image.get(node, model_node_field::next))
    {
        image.set(node, model_node_field::type_name, image.word(node) + base);
        bind_runtime_node_type(image, node, types);
        dispatch_runtime_node_relocation(image, image.get(node, model_node_field::descriptor), node, base, gpu_base,
            types);
        if (image.get(node, model_node_field::child) != 0)
        {
            relocate_runtime_node_chain(image, image.get(node, model_node_field::child), base, gpu_base, types);
        }
    }
}

// Xbox 0x00156790. FUN_00156DD0 supplies the already initialized type registry.
void relocate_runtime_model_tree(const RuntimeRelocationImage& image, u32 tree,
    u32 base, u32 gpu_base, const RuntimeNodeRelocationTypes& types)
{
    if (image.get(tree, model_tree_field::names) != 0)
    {
        const u32 names = image.get(tree, model_tree_field::names) + base;
        image.set(tree, model_tree_field::names, names);
        relocate_runtime_model_names(image, names, base);
    }
    if (image.get(tree, model_tree_field::lists) != 0)
    {
        const u32 lists = image.get(tree, model_tree_field::lists) + base;
        image.set(tree, model_tree_field::lists, lists);
        relocate_runtime_model_pointer_lists(image, lists, base);
    }
    u32 index = 0;
    image.set(tree, model_tree_field::roots, image.get(tree, model_tree_field::roots) + base);
    if (image.get(tree, model_tree_field::root_count) != 0)
    {
        do
        {
            const u32 root = image.get(tree, model_tree_field::roots) + index * 4;
            image.set_word(root, 0, image.word(root) + base);
            relocate_runtime_node_chain(image, image.word(root), base, gpu_base, types);
            ++index;
        } while (index < image.get(tree, model_tree_field::root_count));
    }
    if (image.get(tree, model_tree_field::second_lists) != 0)
    {
        const u32 lists = image.get(tree, model_tree_field::second_lists) + base;
        image.set(tree, model_tree_field::second_lists, lists);
        relocate_runtime_model_pointer_lists(image, lists, base);
    }
}

// Xbox 0x00052710.
void relocate_runtime_texture_table(const RuntimeRelocationImage& image, u32 table,
    u32 base, const RuntimeGpuSegments& gpu)
{
    image.set(table, model_texture_table_field::textures, image.get(table, model_texture_table_field::textures) + base);
    u32 index = 0;
    if (image.word(table) != 0)
    {
        do
        {
            const u32 pointer = image.get(table, model_texture_table_field::textures) + index * 4;
            image.set_word(pointer, 0, image.word(pointer) + base);
            const u32 texture = image.word(pointer);
            if (image.get(texture, model_texture_field::frame_size) == 0)
            {
                image.set(texture, model_texture_field::data, 0);
            }
            else
            {
                image.set(texture, model_texture_field::data,
                    runtime_gpu_address(gpu, image.get(texture, model_texture_field::data)));
            }
            ++index;
        } while (index < image.word(table));
    }
}

// Xbox 0x0004F500. The block arrives in ESI.
void relocate_runtime_geometry_block(const RuntimeRelocationImage& image, u32 block,
    u32 base, const RuntimeGpuSegments& gpu, const RuntimeNodeRelocationBoundaries& boundaries)
{
    if (image.get(block, model_geometry_block_field::unk_08) != 0)
    {
        image.set(block, model_geometry_block_field::unk_08,
            image.get(block, model_geometry_block_field::unk_08) + base);
    }
    if (image.get(block, model_geometry_block_field::unk_0C) != 0)
    {
        image.set(block, model_geometry_block_field::unk_0C,
            image.get(block, model_geometry_block_field::unk_0C) + base);
    }
    if (image.get(block, model_geometry_block_field::streams) != 0)
    {
        image.set(block, model_geometry_block_field::streams,
            image.get(block, model_geometry_block_field::streams) + base);
        u32 index = 0;
        if (image.get(block, model_geometry_block_field::stream_count) != 0)
        {
            u32 offset = 0;
            do
            {
                const u32 stream = image.get(block, model_geometry_block_field::streams) + offset;
                const u32 gpu_offset = image.get(stream, model_geometry_stream_field::data);
                if (gpu_offset != 0)
                {
                    image.set(stream, model_geometry_stream_field::data, runtime_gpu_address(gpu, gpu_offset));
                    boundaries.set_vertex_buffer_header(image.get(stream, model_geometry_stream_field::length), 8, 0, 0,
                        stream + model_geometry_stream_field::header_offset, 0);
                    boundaries.register_resource(stream + model_geometry_stream_field::header_offset,
                        image.get(stream, model_geometry_stream_field::data));
                }
                ++index;
                offset += model_geometry_stream_field::kSize;
            } while (index < image.get(block, model_geometry_block_field::stream_count));
        }
    }
    if (image.get(block, model_geometry_block_field::unk_40) != 0)
    {
        image.set(block, model_geometry_block_field::unk_40,
            image.get(block, model_geometry_block_field::unk_40) + base);
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
    if (image.get(record, model_posed_mesh_field::triangles) != 0)
    {
        image.set(record, model_posed_mesh_field::triangles, image.get(record, model_posed_mesh_field::triangles) + base);
    }
}

// Xbox 0x000536F0.
void relocate_runtime_physics_pairs(const RuntimeRelocationImage& image, u32 record, u32 base)
{
    i32 index = 0;
    if (image.get(record, model_grid_mesh_field::cell_count) != 0)
    {
        u32 pointer = record + model_grid_mesh_field::cells.offset + model_grid_cell_field::triangles.offset;
        do
        {
            if (image.word(pointer) != 0)
            {
                image.set_word(pointer, 0, image.word(pointer) + base);
            }
            ++index;
            pointer += model_grid_mesh_field::cells.stride;
        } while (index < i32(image.get(record, model_grid_mesh_field::cell_count)));
    }
}

// Xbox 0x00055620. The three records at 0x0042380C have stride 0x14.
void relocate_runtime_physics_block(const RuntimeRelocationImage& image, u32 block, u32 base)
{
    using Relocator = void (*)(const RuntimeRelocationImage&, u32, u32);
    static constexpr std::array<Relocator, 3> callbacks{
        relocate_runtime_physics_simple, relocate_runtime_physics_pairs, relocate_runtime_physics_simple};
    image.set(block, model_physics_block_field::unk_0C, image.get(block, model_physics_block_field::unk_0C) + base);
    u32 record = block + model_physics_block_field::records_offset;
    if (image.get(block, model_physics_block_field::count) > 0)
    {
        i32 index = 0;
        do
        {
            const auto callback = callbacks[image.get(record, model_physics_record_field::kind)];
            if (callback != nullptr)
            {
                callback(image, record, base);
            }
            ++index;
            record += image.get(record, model_physics_record_field::size);
        } while (index < image.get(block, model_physics_block_field::count));
    }
}

// Xbox 0x00052910. The tree's pointer base is its own relocated address,
// whereas other directory blocks use the CPU allocation's base.
void relocate_runtime_model_directory(const RuntimeRelocationImage& image,
    u32 directory, u32 base, const RuntimeGpuSegments* gpu,
    const RuntimeNodeRelocationTypes& types, const RuntimeNodeRelocationBoundaries& boundaries)
{
    image.set(directory, model_directory_field::entries, image.word(directory) + base);
    u32 index = 0;
    if (image.get(directory, model_directory_field::count) != 0)
    {
        do
        {
            u32 entry = image.word(directory) + index * 8;
            u32 block;
            switch (image.word(entry))
            {
            case 0:
                image.set(entry, model_directory_entry_field::block,
                    image.get(entry, model_directory_entry_field::block) + base);
                block = image.get(entry, model_directory_entry_field::block);
                image.set(block, model_tree_block_field::tree, image.get(block, model_tree_block_field::tree) + base);
                if (gpu != nullptr)
                {
                    image.set(block, model_tree_block_field::gpu_base,
                        runtime_gpu_address(*gpu, image.get(block, model_tree_block_field::gpu_base)));
                }
                relocate_runtime_model_tree(image, image.get(block, model_tree_block_field::tree),
                    image.get(block, model_tree_block_field::tree), image.get(block, model_tree_block_field::gpu_base),
                    types);
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
                image.set(entry, model_directory_entry_field::block,
                    image.get(entry, model_directory_entry_field::block) + base);
                break;
            case 6:
                image.set(entry, model_directory_entry_field::block,
                    image.get(entry, model_directory_entry_field::block) + base);
                relocate_runtime_physics_block(image, image.get(entry, model_directory_entry_field::block), base);
                break;
            case 7:
                image.set(entry, model_directory_entry_field::block,
                    image.get(entry, model_directory_entry_field::block) + base);
                relocate_runtime_texture_table(image, image.get(entry, model_directory_entry_field::block), base, *gpu);
                break;
            case 0xB:
            case 0x17:
                image.set(entry, model_directory_entry_field::block,
                    image.get(entry, model_directory_entry_field::block) + base);
                block = image.get(entry, model_directory_entry_field::block);
                if (image.word(block, 0xC) != 0)
                {
                    image.set_word(block, 0xC, image.word(block, 0xC) + base);
                }
                break;
            case 0xC:
            case 0xD:
                image.set(entry, model_directory_entry_field::block,
                    image.get(entry, model_directory_entry_field::block) + base);
                block = image.get(entry, model_directory_entry_field::block);
                if (image.word(block, 4) != 0)
                {
                    image.set_word(block, 4, image.word(block, 4) + base);
                }
                relocate_runtime_texture_table(image, block + 8, base, *gpu);
                break;
            case 0xE:
                image.set(entry, model_directory_entry_field::block,
                    image.get(entry, model_directory_entry_field::block) + base);
                relocate_runtime_model_type14(image, image.get(entry, model_directory_entry_field::block), base);
                break;
            case 0x10:
                image.set(entry, model_directory_entry_field::block,
                    image.get(entry, model_directory_entry_field::block) + base);
                block = image.get(entry, model_directory_entry_field::block);
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
                image.set(entry, model_directory_entry_field::block,
                    image.get(entry, model_directory_entry_field::block) + base);
                relocate_runtime_model_type17(image, image.get(entry, model_directory_entry_field::block), base);
                break;
            case 0x12:
                image.set(entry, model_directory_entry_field::block,
                    image.get(entry, model_directory_entry_field::block) + base);
                relocate_runtime_geometry_block(image, image.get(entry, model_directory_entry_field::block), base, *gpu,
                    boundaries);
                break;
            case 0x13:
                image.set(entry, model_directory_entry_field::block,
                    image.get(entry, model_directory_entry_field::block) + base);
                block = image.get(entry, model_directory_entry_field::block);
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
                image.set(entry, model_directory_entry_field::block,
                    image.get(entry, model_directory_entry_field::block) + base);
                relocate_runtime_model_type22(image, image.get(entry, model_directory_entry_field::block), base);
                break;
            case 0x18:
                image.set(entry, model_directory_entry_field::block,
                    image.get(entry, model_directory_entry_field::block) + base);
                relocate_runtime_model_type24(image, image.get(entry, model_directory_entry_field::block), base);
                break;
            case 0x19:
            case 0x1A:
                image.set(entry, model_directory_entry_field::block,
                    image.get(entry, model_directory_entry_field::block) + base);
                block = image.get(entry, model_directory_entry_field::block);
                if (image.word(block, 4) != 0)
                {
                    image.set_word(block, 4, image.word(block, 4) + base);
                }
                break;
            }
            ++index;
        } while (index < image.get(directory, model_directory_field::count));
    }
}

// Xbox 0x00155DA0.
void postload_runtime_matrix_array(const RuntimeRelocationImage& image, u32 node,
    const RuntimeModelHeapBoundaries& heap)
{
    if (image.get(node, matrix_array_node::matrix_count) != 0 && image.get(node, matrix_array_node::matrices) != 0)
    {
        const auto allocation = heap.allocate(image.get(node, matrix_array_node::matrix_count) << 6);
        u32 source = image.get(node, matrix_array_node::matrices);
        u32 destination = 0;
        for (u32 count = (image.get(node, matrix_array_node::matrix_count) & 0x3FFFFFF) << 4; count != 0; --count)
        {
            const u32 value = image.word(source);
            std::memcpy(allocation.bytes.data() + destination, &value, 4);
            source += 4;
            destination += 4;
        }
        image.set(node, matrix_array_node::matrices, allocation.address);
        image.set(node, matrix_array_node::owns_matrices, 1);
        return;
    }
    image.set(node, matrix_array_node::owns_matrices, 0);
}

// Xbox 0x00155DF0.
void unload_runtime_matrix_array(const RuntimeRelocationImage& image, u32 node,
    const RuntimeModelHeapBoundaries& heap)
{
    if (image.get(node, matrix_array_node::matrices) != 0 && image.get(node, matrix_array_node::owns_matrices) != 0)
    {
        heap.free(image.get(node, matrix_array_node::matrices));
        image.set(node, matrix_array_node::matrices, node + image.get(node, matrix_array_node::matrices_offset));
    }
}

// Xbox 0x001581D0. FUN_00155D00 selects the physical allocator callback.
void postload_runtime_push_buffer(const RuntimeRelocationImage& image, u32 node,
    const RuntimeModelHeapBoundaries& heap,
    const std::function<void(u32, u32, u32, u32)>& set_push_buffer_header,
    const RuntimeNodeRelocationBoundaries& d3d)
{
    if (image.get(node, push_buffer_node::header) != 0)
    {
        const auto allocation = heap.allocate_physical(image.get(node, push_buffer_node::size), 0x80,
            ".\\nodes\\geometry\\pushbuffer.c", 0xF4);
        const u32 size = image.get(node, push_buffer_node::size);
        u32 source = image.get(node, push_buffer_node::data);
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
        image.set(node, push_buffer_node::data, allocation.address);
        set_push_buffer_header(image.get(node, push_buffer_node::size), 0, image.get(node, push_buffer_node::header),
            0);
        d3d.register_resource(image.get(node, push_buffer_node::header), image.get(node, push_buffer_node::data));
    }
}

// Xbox 0x0015B360.
void postload_runtime_pixel_shader(const RuntimeRelocationImage& image, u32 shader,
    const RuntimeNodeRelocationBoundaries& d3d)
{
    u32 handle = image.get(shader, model_pixel_shader_field::handle);
    d3d.create_pixel_shader(image.get(shader, model_pixel_shader_field::program), handle);
    image.set(shader, model_pixel_shader_field::handle, handle);
}

// Xbox 0x0015B640.
void unload_runtime_pixel_shader(const RuntimeRelocationImage& image, u32 shader,
    const RuntimeNodeRelocationBoundaries& d3d)
{
    if (image.get(shader, model_pixel_shader_field::handle) != 0)
    {
        d3d.delete_pixel_shader(image.get(shader, model_pixel_shader_field::handle));
        image.set(shader, model_pixel_shader_field::handle, 0);
    }
}

// Xbox 0x00157CB0.
void postload_runtime_shader_node(const RuntimeRelocationImage& image, u32 node,
    const RuntimeNodeRelocationBoundaries& d3d)
{
    postload_runtime_pixel_shader(image, image.get(node, shader_node::primary), d3d);
    if (image.get(node, shader_node::secondary) != 0)
    {
        postload_runtime_pixel_shader(image, image.get(node, shader_node::secondary), d3d);
    }
}

// Xbox 0x00157D00.
void unload_runtime_shader_node(const RuntimeRelocationImage& image, u32 node,
    const RuntimeNodeRelocationBoundaries& d3d)
{
    unload_runtime_pixel_shader(image, image.get(node, shader_node::primary), d3d);
    if (image.get(node, shader_node::secondary) != 0)
    {
        unload_runtime_pixel_shader(image, image.get(node, shader_node::secondary), d3d);
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
        if (image.get(node, model_node_field::child) != 0)
        {
            postload_runtime_node_chain(image, image.get(node, model_node_field::child), heap, d3d);
        }
        node = image.get(node, model_node_field::next);
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
        if (image.get(node, model_node_field::child) != 0)
        {
            unload_runtime_node_chain(image, image.get(node, model_node_field::child), heap, d3d);
        }
        node = image.get(node, model_node_field::next);
    } while (true);
}

// Xbox 0x00156840.
void postload_runtime_model_tree(const RuntimeRelocationImage& image, u32 tree,
    const RuntimeModelHeapBoundaries& heap, const RuntimeNodeRelocationBoundaries& d3d)
{
    u32 index = 0;
    if (image.get(tree, model_tree_field::root_count) != 0)
    {
        do
        {
            postload_runtime_node_chain(image, image.word(image.get(tree, model_tree_field::roots) + index * 4), heap,
                d3d);
            ++index;
        } while (index < image.get(tree, model_tree_field::root_count));
    }
}

// Xbox 0x00156870.
void unload_runtime_model_tree(const RuntimeRelocationImage& image, u32 tree,
    const RuntimeModelHeapBoundaries& heap, const RuntimeNodeRelocationBoundaries& d3d)
{
    u32 index = 0;
    if (image.get(tree, model_tree_field::root_count) != 0)
    {
        do
        {
            unload_runtime_node_chain(image, image.word(image.get(tree, model_tree_field::roots) + index * 4), heap,
                d3d);
            ++index;
        } while (index < image.get(tree, model_tree_field::root_count));
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
    if (image.get(shader, model_material_field::stages) != 0)
    {
        image.set(shader, model_material_field::stages, image.get(shader, model_material_field::stages) + base);
    }
    if (image.get(shader, model_material_field::colours) != 0)
    {
        image.set(shader, model_material_field::colours, image.get(shader, model_material_field::colours) + base);
    }
    if (image.get(shader, model_material_field::constants) != 0)
    {
        image.set(shader, model_material_field::constants, image.get(shader, model_material_field::constants) + base);
    }
    if (image.get(shader, model_material_field::name) != 0)
    {
        image.set(shader, model_material_field::name, image.get(shader, model_material_field::name) + base);
    }
    if (image.get(shader, model_material_field::entries) != 0)
    {
        image.set(shader, model_material_field::entries, image.get(shader, model_material_field::entries) + base);
        u32 index = 0;
        if (image.get(shader, model_material_field::entry_count) != 0)
        {
            u32 offset = 0;
            do
            {
                const u32 entry = image.get(shader, model_material_field::entries) + offset;
                image.set(entry, model_material_entry_field::name,
                    image.get(entry, model_material_entry_field::name) + base);
                ++index;
                offset += model_material_entry_field::kSize;
            } while (index < image.get(shader, model_material_field::entry_count));
        }
    }
}

// Xbox 0x0015B340.
void relocate_runtime_pixel_shader(const RuntimeRelocationImage& image, u32 shader, u32 base)
{
    image.set(shader, model_pixel_shader_field::program, image.get(shader, model_pixel_shader_field::program) + base);
}

// Xbox 0x00158910.
void relocate_runtime_node_links(const RuntimeRelocationImage& image, u32 node, u32 base)
{
    if (image.get(node, model_node_field::child) != 0)
    {
        image.set(node, model_node_field::child, image.get(node, model_node_field::child) + base);
    }
    if (image.get(node, model_node_field::next) != 0)
    {
        image.set(node, model_node_field::next, image.get(node, model_node_field::next) + base);
    }
    if (image.get(node, model_node_field::unk_1C) != 0)
    {
        image.set(node, model_node_field::unk_1C, image.get(node, model_node_field::unk_1C) + base);
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
    if (image.get(node, push_buffer_node::header) != 0)
    {
        image.set(node, push_buffer_node::header, image.get(node, push_buffer_node::header) + base);
        image.set(node, push_buffer_node::data, image.get(node, push_buffer_node::data) + base);
        return;
    }
    image.set(node, push_buffer_node::draw_indices, image.get(node, push_buffer_node::draw_indices) + base);
    image.set(node, push_buffer_node::draw_counts, image.get(node, push_buffer_node::draw_counts) + base);
    u32 index = 0;
    image.set(node, push_buffer_node::draw_primitives, image.get(node, push_buffer_node::draw_primitives) + base);
    if (image.get(node, push_buffer_node::draw_count) != 0)
    {
        do
        {
            const u32 entry = image.get(node, push_buffer_node::draw_indices) + index * 4;
            image.set_word(entry, 0, image.word(entry) + base);
            ++index;
        } while (index < image.get(node, push_buffer_node::draw_count));
    }
}

// Xbox 0x001580A0.
void relocate_runtime_background_push_buffer_node(const RuntimeRelocationImage& image, u32 node, u32 base)
{
    image.set(node, push_buffer_node::unk_44, image.get(node, push_buffer_node::unk_44) + base);
    image.set(node, push_buffer_node::unk_40, image.get(node, push_buffer_node::unk_40) + base);
    relocate_runtime_node_links(image, node, base);
    if (image.get(node, push_buffer_node::header) != 0)
    {
        image.set(node, push_buffer_node::header, image.get(node, push_buffer_node::header) + base);
        image.set(node, push_buffer_node::data, image.get(node, push_buffer_node::data) + base);
        return;
    }
    image.set(node, push_buffer_node::draw_indices, image.get(node, push_buffer_node::draw_indices) + base);
    image.set(node, push_buffer_node::draw_counts, image.get(node, push_buffer_node::draw_counts) + base);
    u32 index = 0;
    image.set(node, push_buffer_node::draw_primitives, image.get(node, push_buffer_node::draw_primitives) + base);
    if (image.get(node, push_buffer_node::draw_count) != 0)
    {
        do
        {
            const u32 entry = image.get(node, push_buffer_node::draw_indices) + index * 4;
            image.set_word(entry, 0, image.word(entry) + base);
            ++index;
        } while (index < image.get(node, push_buffer_node::draw_count));
    }
}

// Xbox 0x001569A0.
void relocate_runtime_skeleton_node(const RuntimeRelocationImage& image, u32 node, u32 base)
{
    relocate_runtime_node_links(image, node, base);
    image.set(node, skeleton_node::joints, image.get(node, skeleton_node::joints) + base);
}

// Xbox 0x001572D0.
void relocate_runtime_shader_parameter_node(const RuntimeRelocationImage& image, u32 node, u32 base)
{
    u32 shader = image.get(node, shader_node::primary) + base;
    image.set(node, shader_node::primary, shader);
    relocate_runtime_shader_parameters(image, shader, base);
    if (image.get(node, shader_node::secondary) != 0)
    {
        shader = image.get(node, shader_node::secondary) + base;
        image.set(node, shader_node::secondary, shader);
        relocate_runtime_shader_parameters(image, shader, base);
    }
    relocate_runtime_node_links(image, node, base);
}

// Xbox 0x00157C60.
void relocate_runtime_shader_node(const RuntimeRelocationImage& image, u32 node, u32 base)
{
    u32 shader = image.get(node, shader_node::primary) + base;
    image.set(node, shader_node::primary, shader);
    relocate_runtime_pixel_shader(image, shader, base);
    if (image.get(node, shader_node::secondary) != 0)
    {
        shader = image.get(node, shader_node::secondary) + base;
        image.set(node, shader_node::secondary, shader);
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
    const u32 matrices = image.get(node, matrix_array_node::matrices) + base;
    image.set(node, matrix_array_node::rows, image.get(node, matrix_array_node::rows) + base);
    image.set(node, matrix_array_node::matrices, matrices);
    if (matrices != 0 && image.get(node, matrix_array_node::matrix_count) != 0)
    {
        const i32 offset = std::bit_cast<i32>(matrices - node);
        if (offset < 0 || offset > 0xFFFF)
        {
            runtime_model_noop();
        }
        image.set(node, matrix_array_node::matrices_offset, u16(offset));
    }
}

// Xbox 0x00157C40.
void relocate_runtime_rigid_skin_node(const RuntimeRelocationImage& image, u32 node, u32 base)
{
    image.set(node, rigid_skin_node::indices, image.get(node, rigid_skin_node::indices) + base);
    relocate_runtime_node_links(image, node, base);
}

} // namespace gbtg
