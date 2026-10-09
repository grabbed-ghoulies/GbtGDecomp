#pragma once

#include "gbtg/types.h"
#include <cstddef>
#include <functional>

namespace gbtg
{

// A host representation of Xbox pointers. Addition stays unsigned 32-bit;
// dereference maps back into the CPU allocation without widening its fields.
struct RuntimeRelocationImage
{
    std::span<u8> bytes;
    u32 address;

    u8* pointer(u32 value) const
    {
        return bytes.data() + u32(value - address);
    }
    u32 word(u32 value, u32 offset = 0) const
    {
        u32 result;
        std::memcpy(&result, pointer(value + offset), 4);
        return result;
    }
    u16 half(u32 value, u32 offset = 0) const
    {
        u16 result;
        std::memcpy(&result, pointer(value + offset), 2);
        return result;
    }
    u8 byte(u32 value, u32 offset = 0) const
    {
        return *pointer(value + offset);
    }
    void set_word(u32 value, u32 offset, u32 data) const
    {
        std::memcpy(pointer(value + offset), &data, 4);
    }
    void set_half(u32 value, u32 offset, u16 data) const
    {
        std::memcpy(pointer(value + offset), &data, 2);
    }
    void set_byte(u32 value, u32 offset, u8 data) const
    {
        *pointer(value + offset) = data;
    }
};
struct RuntimeGpuSegment
{
    u32 address; // GPU allocation +8, stride 8
    u32 size; // +0xC
};
struct RuntimeGpuSegments
{
    std::span<const RuntimeGpuSegment> segments; // count +4
};

struct RuntimeVertexShaderCacheEntry
{
    std::array<u32, 545> program{};
    // +0x884: declaration words followed by six handle/key pairs at +0x904.
    // Keep the original contiguous word view for the declaration-copy loop.
    std::array<i32, 44> declaration{};
    i32 variant_count = 0; // +0x934
    i32 key = 0;
    i32 first_channel = 0;
    i32 second_channel = 0;
};
static_assert(sizeof(RuntimeVertexShaderCacheEntry) == 0x944);
static_assert(offsetof(RuntimeVertexShaderCacheEntry, declaration) == 0x884);
static_assert(offsetof(RuntimeVertexShaderCacheEntry, variant_count) == 0x934);
static_assert(offsetof(RuntimeVertexShaderCacheEntry, key) == 0x938);
struct RuntimeVertexShaderCache
{
    std::array<RuntimeVertexShaderCacheEntry, 256> entries{};
    i32 count = 0;
};

i32 intern_runtime_vertex_shader(RuntimeVertexShaderCache& cache,
    const RuntimeRelocationImage& image, u32 program, u32 declaration,
    i32 key, i32 first_channel, i32 second_channel); // Xbox 0x00159430
void relocate_runtime_vertex_shader_node(const RuntimeRelocationImage& image,
    u32 node, u32 base, RuntimeVertexShaderCache& cache); // Xbox 0x001584A0

struct RuntimeNodeRelocationBoundaries
{
    std::function<void(u32 resource, u32 data)> register_resource;
    std::function<void(u32 output, u32 input)> transpose;
    std::function<void(u32 length, u32 usage, u32 format, u32 pool,
        u32 resource, u32 data)> set_vertex_buffer_header;
    std::function<void(u32 source_name, u32 source, u32 length, u32 flags,
        u32 constants, u32& buffer, u32 errors, u32 listing, u32 shader_type,
        u32 options, u32 reserved)> assemble_shader;
    std::function<u32(u32 buffer)> assembled_shader_data;
    std::function<void(u32 data, u32& handle)> create_pixel_shader;
    std::function<void(u32 handle)> delete_pixel_shader;
    std::function<void(u32 size, u32 usage, u32 resource, u32 data)> set_push_buffer_header;
    std::function<void(u32 width, u32 height, u32 levels, u32 usage, u32 format,
        u32 pool, u32 texture, u32 data, u32 pitch)> set_texture_header;
    std::function<std::span<u8>(u32 texture, u32 level, u32 rectangle, u32 flags)> lock_texture;
    std::function<void(u32 resource)> release_resource;
};
struct RuntimeModelAllocation
{
    u32 address;
    std::span<u8> bytes;
};
struct RuntimeModelHeapBoundaries
{
    std::function<RuntimeModelAllocation(u32 size)> allocate;
    std::function<RuntimeModelAllocation(u32 size, u32 alignment,
        const char* file, u32 line)> allocate_physical;
    std::function<void(u32 address)> free;
};
void postload_runtime_matrix_array(const RuntimeRelocationImage& image, u32 node,
    const RuntimeModelHeapBoundaries& heap); // Xbox 0x00155DA0
void unload_runtime_matrix_array(const RuntimeRelocationImage& image, u32 node,
    const RuntimeModelHeapBoundaries& heap); // Xbox 0x00155DF0
void postload_runtime_push_buffer(const RuntimeRelocationImage& image, u32 node,
    const RuntimeModelHeapBoundaries& heap,
    const std::function<void(u32, u32, u32, u32)>& set_push_buffer_header,
    const RuntimeNodeRelocationBoundaries& d3d); // Xbox 0x001581D0
void postload_runtime_pixel_shader(const RuntimeRelocationImage& image, u32 shader,
    const RuntimeNodeRelocationBoundaries& d3d); // Xbox 0x0015B360
void unload_runtime_pixel_shader(const RuntimeRelocationImage& image, u32 shader,
    const RuntimeNodeRelocationBoundaries& d3d); // Xbox 0x0015B640
void postload_runtime_shader_node(const RuntimeRelocationImage& image, u32 node,
    const RuntimeNodeRelocationBoundaries& d3d); // Xbox 0x00157CB0
void unload_runtime_shader_node(const RuntimeRelocationImage& image, u32 node,
    const RuntimeNodeRelocationBoundaries& d3d); // Xbox 0x00157D00
void postload_runtime_node_chain(const RuntimeRelocationImage& image, u32 node,
    const RuntimeModelHeapBoundaries& heap,
    const RuntimeNodeRelocationBoundaries& d3d); // Xbox 0x00156660
void unload_runtime_node_chain(const RuntimeRelocationImage& image, u32 node,
    const RuntimeModelHeapBoundaries& heap,
    const RuntimeNodeRelocationBoundaries& d3d); // Xbox 0x00156720
void postload_runtime_model_tree(const RuntimeRelocationImage& image, u32 tree,
    const RuntimeModelHeapBoundaries& heap,
    const RuntimeNodeRelocationBoundaries& d3d); // Xbox 0x00156840
void unload_runtime_model_tree(const RuntimeRelocationImage& image, u32 tree,
    const RuntimeModelHeapBoundaries& heap,
    const RuntimeNodeRelocationBoundaries& d3d); // Xbox 0x00156870
void relocate_runtime_vertex_buffer_node(const RuntimeRelocationImage& image,
    u32 node, u32 base, u32 gpu_base,
    const RuntimeNodeRelocationBoundaries& boundaries); // Xbox 0x001583C0
void relocate_runtime_transform_node(const RuntimeRelocationImage& image,
    u32 node, u32 base, const RuntimeNodeRelocationBoundaries& boundaries); // Xbox 0x00157EA0
void relocate_runtime_blendshape_node(const RuntimeRelocationImage& image,
    u32 node, u32 base, u32 gpu_base,
    const RuntimeNodeRelocationBoundaries& boundaries); // Xbox 0x001573A0

struct RuntimeNodeRelocationType
{
    std::string name;
    std::function<void(const RuntimeRelocationImage&, u32, u32, u32)> relocate;
};
struct RuntimeNodeRelocationTypes
{
    u32 address;
    std::span<const RuntimeNodeRelocationType> entries;
};
// The relocation slots registered by 0x00155A10; other renderer initialization
// remains separate. Captured cache, shader and D3D state must outlive the table.
std::array<RuntimeNodeRelocationType, 17> runtime_model_relocation_types(
    RuntimeVertexShaderCache& cache, u32& default_pixel_shader,
    const RuntimeNodeRelocationBoundaries& d3d);
void bind_runtime_node_type(const RuntimeRelocationImage& image, u32 node,
    const RuntimeNodeRelocationTypes& types); // Xbox 0x00158770
void dispatch_runtime_node_relocation(const RuntimeRelocationImage& image, u32 descriptor,
    u32 node, u32 base, u32 gpu_base, const RuntimeNodeRelocationTypes& types); // Xbox 0x001588C0
void relocate_runtime_node_chain(const RuntimeRelocationImage& image, u32 node,
    u32 base, u32 gpu_base, const RuntimeNodeRelocationTypes& types); // Xbox 0x00156600
void relocate_runtime_model_tree(const RuntimeRelocationImage& image, u32 tree,
    u32 base, u32 gpu_base, const RuntimeNodeRelocationTypes& types); // Xbox 0x00156790
void relocate_runtime_texture_table(const RuntimeRelocationImage& image, u32 table,
    u32 base, const RuntimeGpuSegments& gpu); // Xbox 0x00052710
void relocate_runtime_geometry_block(const RuntimeRelocationImage& image, u32 block,
    u32 base, const RuntimeGpuSegments& gpu,
    const RuntimeNodeRelocationBoundaries& boundaries); // Xbox 0x0004F500
void relocate_runtime_pixel_shader_node(const RuntimeRelocationImage& image,
    u32 node, u32 base, u32& default_pixel_shader,
    const RuntimeNodeRelocationBoundaries& boundaries); // Xbox 0x00158680
void relocate_runtime_physics_simple(const RuntimeRelocationImage& image,
    u32 record, u32 base); // Xbox 0x00054490
void relocate_runtime_physics_pairs(const RuntimeRelocationImage& image,
    u32 record, u32 base); // Xbox 0x000536F0
void relocate_runtime_physics_block(const RuntimeRelocationImage& image,
    u32 block, u32 base); // Xbox 0x00055620
void relocate_runtime_model_directory(const RuntimeRelocationImage& image,
    u32 directory, u32 base, const RuntimeGpuSegments* gpu,
    const RuntimeNodeRelocationTypes& types,
    const RuntimeNodeRelocationBoundaries& boundaries); // Xbox 0x00052910
struct RuntimeSpecialTexture
{
    std::function<u32(u32, u32)> get_texture;
    u32 model = 0;
    u32 index = 0;
    u16 width = 0;
    u16 height = 0;
    u32 format = 0;
};
using RuntimeSpecialTextures = std::array<RuntimeSpecialTexture, 40>;
void postload_runtime_texture(const RuntimeRelocationImage& image, u32 texture,
    const RuntimeNodeRelocationBoundaries& d3d); // Xbox 0x000534A0
u32 lookup_runtime_model_texture(const RuntimeRelocationImage& image,
    u32 directory, u32 index); // Xbox 0x000528C0
void clear_runtime_special_texture(RuntimeSpecialTextures& textures, u32 code,
    const RuntimeNodeRelocationBoundaries& d3d); // Xbox 0x001417D0
void postload_runtime_texture_table(const RuntimeRelocationImage& image, u32 table,
    u32 directory, RuntimeSpecialTextures& textures,
    const RuntimeNodeRelocationBoundaries& d3d); // Xbox 0x000527C0
void unload_runtime_texture_table(const RuntimeRelocationImage& image, u32 table,
    RuntimeSpecialTextures& textures, const RuntimeNodeRelocationBoundaries& d3d); // Xbox 0x00052850
RuntimeModelAllocation postload_runtime_model_directory(const RuntimeRelocationImage& image,
    u32 directory, RuntimeSpecialTextures& textures, const RuntimeModelHeapBoundaries& heap,
    const RuntimeNodeRelocationBoundaries& d3d); // Xbox 0x00052B70
void unload_runtime_model_directory(const RuntimeRelocationImage& image, u32 directory,
    const RuntimeModelAllocation& runtime, RuntimeSpecialTextures& textures,
    const RuntimeModelHeapBoundaries& heap,
    const RuntimeNodeRelocationBoundaries& d3d); // Xbox 0x00052E50
void release_runtime_model_gpu_cache(const RuntimeRelocationImage& image, u32 instance,
    const RuntimeModelHeapBoundaries& heap,
    const RuntimeNodeRelocationBoundaries& d3d); // Xbox 0x000F9C20

u32 runtime_gpu_address(const RuntimeGpuSegments& gpu, u32 offset); // Xbox 0x00051F20
void relocate_runtime_model_names(const RuntimeRelocationImage& image, u32 names,
    u32 base); // Xbox 0x001568A0
void relocate_runtime_model_pointer_lists(const RuntimeRelocationImage& image, u32 lists,
    u32 base); // Xbox 0x00158B50
void relocate_runtime_model_type14(const RuntimeRelocationImage& image, u32 block,
    u32 base); // Xbox 0x00053320
void relocate_runtime_model_type17(const RuntimeRelocationImage& image, u32 block,
    u32 base); // Xbox 0x000531D0
void relocate_runtime_model_type22(const RuntimeRelocationImage& image, u32 block,
    u32 base); // Xbox 0x00051F60
void relocate_runtime_model_type24(const RuntimeRelocationImage& image, u32 block,
    u32 base); // Xbox 0x00053270
void relocate_runtime_shader_parameters(const RuntimeRelocationImage& image, u32 shader,
    u32 base); // Xbox 0x0015ACB0
void relocate_runtime_pixel_shader(const RuntimeRelocationImage& image, u32 shader,
    u32 base); // Xbox 0x0015B340
void relocate_runtime_node_links(const RuntimeRelocationImage& image, u32 node,
    u32 base); // Xbox 0x00158910
void relocate_runtime_pixel_shader_parameter_node(const RuntimeRelocationImage& image, u32 node,
    u32 base); // Xbox 0x00158720
void relocate_runtime_push_buffer_node(const RuntimeRelocationImage& image, u32 node,
    u32 base); // Xbox 0x00158160
void relocate_runtime_background_push_buffer_node(const RuntimeRelocationImage& image, u32 node,
    u32 base); // Xbox 0x001580A0
void relocate_runtime_skeleton_node(const RuntimeRelocationImage& image, u32 node,
    u32 base); // Xbox 0x001569A0
void relocate_runtime_shader_parameter_node(const RuntimeRelocationImage& image, u32 node,
    u32 base); // Xbox 0x001572D0
void relocate_runtime_shader_node(const RuntimeRelocationImage& image, u32 node,
    u32 base); // Xbox 0x00157C60
void relocate_runtime_matrix_array_node(const RuntimeRelocationImage& image, u32 node,
    u32 base); // Xbox 0x00155D50
void relocate_runtime_rigid_skin_node(const RuntimeRelocationImage& image, u32 node,
    u32 base); // Xbox 0x00157C40
void runtime_model_noop(); // Xbox 0x001628B0: the original callback returns immediately

} // namespace gbtg
