#include "gbtg/model_relocation.h"
#include "gbtg/model_views.h"
#include <array>
#include <bit>
#include <iostream>
#include <memory>
#include <stdexcept>

using namespace gbtg;

namespace
{
void require(bool value, const char* message)
{
    if (!value)
    {
        throw std::runtime_error(message);
    }
}

void test_gpu_ranges()
{
    const std::array<RuntimeGpuSegment, 3> ranges{{{0x8000, 16}, {0xA000, 0}, {0xC000, 32}}};
    const RuntimeGpuSegments gpu{ranges};
    require(runtime_gpu_address(gpu, 15) == 0x800F, "first chunk end");
    require(runtime_gpu_address(gpu, 16) == 0xC000, "strict boundary and empty chunk");
    require(runtime_gpu_address(gpu, 47) == 0xC01F, "last chunk end");
    require(runtime_gpu_address(gpu, 48) == 0, "unmapped offset");
    require(runtime_gpu_address(RuntimeGpuSegments{}, 0) == 0, "empty GPU table");
}

void test_names_and_links()
{
    std::array<u8, 512> data{};
    const RuntimeRelocationImage image{data, 0xFFFFFF00};
    const u32 base = image.address;
    image.set_word(base, 0, 2);
    image.set_word(base, 4, 0x40);
    image.set_word(base + 0x40, 0, 0);
    image.set_word(base + 0x40, 4, 0x180);
    image.set_word(base + 0x40, 8, 0x12345678);
    image.set_word(base + 0x4C, 0, 0x190);
    image.set_word(base + 0x4C, 4, 0x1A0);
    relocate_runtime_model_names(image, base, base);
    require(image.word(base, 4) == base + 0x40, "table relocation");
    require(image.word(base + 0x40) == base, "zero named value is unconditionally rebased");
    require(image.word(base + 0x40, 4) == 0x80, "32-bit wrapped name pointer");
    require(image.word(base + 0x40, 8) == 0x12345678, "name entry tail retained");
    require(image.word(base + 0x4C) == 0x90, "second name stride");
    const u32 node = base + 0x80;
    image.set_word(node, 0x14, 0);
    image.set_word(node, 0x18, 0x100);
    image.set_word(node, 0x1C, 0x140);
    relocate_runtime_node_links(image, node, base);
    require(image.word(node, 0x14) == 0, "null child retained");
    require(image.word(node, 0x18) == 0, "nonnull relative sibling may wrap to zero");
    require(image.word(node, 0x1C) == 0x40, "node link relocation");
}

void test_matrix_offsets()
{
    std::array<u8, 512> data{};
    const RuntimeRelocationImage image{data, 0x100000};
    const u32 node = image.address + 0x40;
    image.set_word(node, 0x2C, 0x180);
    image.set_word(node, 0x30, 1);
    image.set_byte(node, 0x29, 0xA5);
    relocate_runtime_matrix_array_node(image, node, image.address);
    require(image.word(node, 0x2C) == image.address + 0x180, "matrix pointer");
    require(image.half(node, 0x2A) == 0x140, "matrix relative offset");
    require(image.byte(node, 0x29) == 0xA5, "adjacent ownership byte retained");
    image.set_word(node, 0x2C, 0x20);
    relocate_runtime_matrix_array_node(image, node, image.address);
    require(image.half(node, 0x2A) == 0xFFE0, "negative offset truncates after retail no-op callback");
    image.set_word(node, 0x2C, 0x180);
    image.set_word(node, 0x30, 0);
    relocate_runtime_matrix_array_node(image, node, image.address);
    require(image.half(node, 0x2A) == 0xFFE0, "empty matrix array retains offset");
}

void test_shader_interning()
{
    std::array<u8, 512> data{};
    const RuntimeRelocationImage image{data, 0x100000};
    const u32 program = image.address + 0x100;
    const u32 declaration = image.address + 0x140;
    image.set_word(program, 0, 0x00011234);
    for (u32 index = 1; index < 5; ++index)
    {
        image.set_word(program, index * 4, index * 17);
    }
    image.set_word(declaration, 0, 42);
    image.set_word(declaration, 4, 0xFFFFFFFF);
    auto cache = std::make_unique<RuntimeVertexShaderCache>();
    require(intern_runtime_vertex_shader(*cache, image, program, declaration, 3, 1, 0) == 0,
        "first program insertion");
    image.set_word(declaration, 0, 99);
    require(intern_runtime_vertex_shader(*cache, image, program, declaration, 3, 1, 0) == 0 &&
        cache->count == 1 && cache->entries[0].declaration[0] == 42,
        "declaration is deliberately absent from cache key");
    image.set_word(program, 16, 123);
    require(intern_runtime_vertex_shader(*cache, image, program, declaration, 3, 1, 0) == 1,
        "last program word participates in lookup");
    require(intern_runtime_vertex_shader(*cache, image, program, declaration, 3, 0, 0) == 2,
        "channel flags participate in lookup");
    require(cache->entries[1].declaration[1] == -1 && cache->entries[1].handle == 0,
        "declaration terminator and shader handle initialization");
    const u32 node = image.address;
    image.set_word(node, 0x50, 0x140);
    image.set_word(node, 0x54, 0x100);
    image.set_word(node, 0x44, 3);
    image.set_word(node, 0x3C, 0xFFFFFFFF);
    image.set_word(node, 0x40, 0xFFFFFFFF);
    relocate_runtime_vertex_shader_node(image, node, image.address, *cache);
    require(image.word(node, 0x64) == 2 && cache->count == 3,
        "node relocation uses translated program cache");
}

void test_blendshape_registration()
{
    std::array<u8, 1024> data{};
    const RuntimeRelocationImage image{data, 0x200000};
    const u32 node = image.address;
    image.set_word(node, 0x20, 0x80);
    image.set_half(node, 0x24, 2);
    image.set_word(node + 0x80, 0, 0x100);
    image.set_word(node + 0x80, 4, 0x140);
    image.set_word(node + 0x100, 0x20, 0x200);
    image.set_word(node + 0x100, 0x24, 2);
    image.set_word(node + 0x140, 0x20, 0x280);
    image.set_word(node + 0x140, 0x24, 2);
    for (u32 record : {0x200U, 0x218U, 0x280U, 0x298U})
    {
        image.set_word(node + record, 0x10, record);
    }
    image.set_byte(node + 0x280, 1, 10);
    image.set_byte(node + 0x298, 1, 9);
    std::vector<std::pair<u32, u32>> registrations;
    RuntimeNodeRelocationBoundaries boundaries;
    boundaries.register_resource = [&](u32 resource, u32 address)
    {
        require(image.word(node + 0x298, 0x10) == 0x8298,
            "all shape data must relocate before the first D3D call");
        registrations.emplace_back(resource, address);
    };
    relocate_runtime_blendshape_node(image, node, node, 0x8000, boundaries);
    require(registrations == std::vector<std::pair<u32, u32>>{
        {node + 0x204, 0x8200}, {node + 0x21C, 0x8218}, {node + 0x29C, 0x8298}},
        "first shape registers all streams; subsequent shapes register only type 9");
}

void test_tree_dispatch()
{
    std::array<u8, 1024> data{};
    const RuntimeRelocationImage image{data, 0x100000};
    const u32 base = image.address;
    const char name[] = "ndGroup";
    std::memcpy(image.pointer(base + 0x300), name, sizeof(name));
    image.set_word(base, 8, 0x40);
    image.set_word(base, 0xC, 1);
    image.set_word(base + 0x40, 0, 0x80);
    for (u32 offset : {0x80U, 0x100U, 0x180U})
    {
        image.set_word(base + offset, 0, 0x300);
        image.set_word(base + offset, 4, 0xABCDFFFF);
    }
    image.set_word(base + 0x80, 0x14, 0x100);
    image.set_word(base + 0x80, 0x18, 0x180);
    std::vector<u32> visited;
    const std::array<RuntimeNodeRelocationType, 1> records{{{"ndGroup",
        [&](const RuntimeRelocationImage& input, u32 node, u32 cpu, u32 gpu)
        {
            require(gpu == 0x8000 && input.word(node, 0x10) == 0x500000,
                "descriptor bound before virtual relocation");
            require(input.word(node, 4) == 0xABCD0000,
                "type assignment preserves upper flag half");
            visited.push_back(node);
            relocate_runtime_node_links(input, node, cpu);
        }}}};
    const RuntimeNodeRelocationTypes types{0x500000, records};
    relocate_runtime_model_tree(image, base, base, 0x8000, types);
    require(visited == std::vector<u32>{base + 0x80, base + 0x100, base + 0x180},
        "tree traversal relocates child before sibling");
    image.set_word(base + 0x200, 0, base + 0x300);
    image.set_word(base + 0x200, 0x10, 0x12345678);
    image.set_half(base + 0x200, 4, 17);
    const std::array<RuntimeNodeRelocationType, 1> mismatched{{{"NDGROUP", {}}}};
    bind_runtime_node_type(image, base + 0x200, RuntimeNodeRelocationTypes{0x500000, mismatched});
    require(image.word(base + 0x200, 0x10) == 0x12345678 &&
        image.half(base + 0x200, 4) == 17, "case-sensitive miss retains descriptor and type");
}

void test_directory_pointer_bases()
{
    std::array<u8, 2048> data{};
    const RuntimeRelocationImage image{data, 0x100000};
    const u32 base = image.address;
    image.set_word(base, 0, 0x40);
    image.set_word(base, 4, 7);
    const std::array<u32, 7> ids{0, 3, 7, 6, 0x19, 0x1A, 1};
    const std::array<u32, 7> blocks{0x100, 0x380, 0x400, 0x500, 0x600, 0x640, 0x700};
    for (u32 index = 0; index < ids.size(); ++index)
    {
        image.set_word(base + 0x40 + index * 8, 0, ids[index]);
        image.set_word(base + 0x40 + index * 8, 4, blocks[index]);
    }
    image.set_word(base + 0x100, 0, 0x180);
    image.set_word(base + 0x100, 4, 16);
    const u32 tree = base + 0x180;
    image.set_word(tree, 8, 0x40);
    image.set_word(tree, 0xC, 1);
    image.set_word(tree + 0x40, 0, 0x80);
    image.set_word(tree + 0x80, 0, 0x100);
    std::memcpy(image.pointer(tree + 0x100), "ndGroup", 8);
    u32 visited_node = 0;
    const std::array<RuntimeNodeRelocationType, 1> records{{{"ndGroup",
        [&](const RuntimeRelocationImage& input, u32 node, u32 cpu, u32 gpu)
        {
            require(cpu == tree && gpu == 0xC000, "tree receives its own base and resolved GPU chunk");
            visited_node = node;
            relocate_runtime_node_links(input, node, cpu);
        }}}};
    image.set_word(base + 0x400, 0, 1);
    image.set_word(base + 0x400, 4, 0x440);
    image.set_word(base + 0x440, 0, 0x460);
    image.set_word(base + 0x460, 0x14, 0);
    image.set_word(base + 0x460, 0x18, 4);
    image.set_word(base + 0x500, 0, 1);
    image.set_word(base + 0x500, 0xC, 0x580);
    image.set_byte(base + 0x510, 0, 2);
    image.set_word(base + 0x510, 8, 0x20);
    image.set_word(base + 0x510, 0x14, 0x590);
    image.set_word(base + 0x600, 4, 0x680);
    image.set_word(base + 0x640, 4, 0);
    const std::array<RuntimeGpuSegment, 2> segments{{{0x8000, 16}, {0xC000, 32}}};
    const RuntimeGpuSegments gpu{segments};
    relocate_runtime_model_directory(image, base, base, &gpu,
        RuntimeNodeRelocationTypes{0x500000, records}, RuntimeNodeRelocationBoundaries{});
    require(visited_node == tree + 0x80, "tree-relative node pointer");
    require(image.word(base + 0x4C) == base + 0x380, "plain directory block pointer");
    require(image.word(base + 0x460, 0x14) == 0x8000, "zero GPU offset is valid for texture data");
    require(image.word(base + 0x510, 0x14) == base + 0x590, "physics callback dispatch");
    require(image.word(base + 0x600, 4) == base + 0x680 &&
        image.word(base + 0x640, 4) == 0, "type 25/26 payload pointer gates");
    require(image.word(base + 0x74) == 0x700, "unhandled type 1 remains untouched");
}

void test_postload_ownership()
{
    std::array<u8, 1024> data{};
    const RuntimeRelocationImage image{data, 0x100000};
    const u32 node = image.address;
    const u32 source = node + 0x200;
    for (u32 index = 0; index < 64; ++index)
    {
        image.set_byte(source + index, 0, u8(index));
    }
    image.set_word(node, 0x2C, source);
    image.set_word(node, 0x30, 1);
    image.set_half(node, 0x2A, 0x200);
    std::vector<u8> allocation;
    std::vector<u32> frees;
    RuntimeModelHeapBoundaries heap;
    heap.allocate = [&](u32 size)
    {
        allocation.assign(size, 0xFF);
        return RuntimeModelAllocation{0x900000, allocation};
    };
    heap.free = [&](u32 address)
    {
        frees.push_back(address);
    };
    postload_runtime_matrix_array(image, node, heap);
    require(allocation.size() == 64 && allocation[63] == 63 &&
        image.word(node, 0x2C) == 0x900000 && image.byte(node, 0x29) == 1,
        "matrix data copies into owned allocation");
    unload_runtime_matrix_array(image, node, heap);
    require(frees == std::vector<u32>{0x900000} && image.word(node, 0x2C) == source &&
        image.byte(node, 0x29) == 1, "matrix unload restores pointer and retains ownership byte");
    image.set_word(node, 0x30, 0);
    postload_runtime_matrix_array(image, node, heap);
    require(image.byte(node, 0x29) == 0, "empty matrix postload clears ownership");
    image.set_word(node, 0x24, node + 0x180);
    image.set_word(node, 0x28, source);
    image.set_word(node, 0x2C, 7);
    heap.allocate_physical = [&](u32 size, u32 alignment, const char* file, u32 line)
    {
        require(size == 7 && alignment == 0x80 && line == 0xF4 &&
            std::strcmp(file, ".\\nodes\\geometry\\pushbuffer.c") == 0,
            "push buffer physical allocator arguments");
        allocation.assign(size, 0xFF);
        return RuntimeModelAllocation{0xA00000, allocation};
    };
    std::vector<int> calls;
    RuntimeNodeRelocationBoundaries d3d;
    d3d.set_push_buffer_header = [&](u32 size, u32 usage, u32 resource, u32 address)
    {
        require(size == 7 && usage == 0 && resource == node + 0x180 && address == 0 &&
            image.word(node, 0x28) == 0xA00000, "push pointer published before header setup");
        calls.push_back(1);
    };
    d3d.register_resource = [&](u32 resource, u32 address)
    {
        require(resource == node + 0x180 && address == 0xA00000, "push registration arguments");
        calls.push_back(2);
    };
    postload_runtime_push_buffer(image, node, heap, d3d.set_push_buffer_header, d3d);
    require(allocation == std::vector<u8>{0, 1, 2, 3, 4, 5, 6} && calls == std::vector<int>{1, 2},
        "push buffer copies trailing bytes and preserves D3D call order");
    const u32 shader = node + 0x100;
    image.set_word(shader, 4, source);
    image.set_word(shader, 0xC, 13);
    d3d.create_pixel_shader = [&](u32 shader_data, u32& handle)
    {
        require(shader_data == source && handle == 13, "pixel shader creation output retains old value");
        handle = 17;
    };
    d3d.delete_pixel_shader = [&](u32 handle)
    {
        require(handle == 17, "pixel shader release handle");
        calls.push_back(3);
    };
    postload_runtime_pixel_shader(image, shader, d3d);
    unload_runtime_pixel_shader(image, shader, d3d);
    unload_runtime_pixel_shader(image, shader, d3d);
    require(image.word(shader, 0xC) == 0 && calls == std::vector<int>{1, 2, 3},
        "pixel shader deletion clears handle and does not repeat");
}

void test_texture_lifecycle()
{
    std::array<u8, 1024> data{};
    const RuntimeRelocationImage image{data, 0x100000};
    const u32 directory = image.address;
    const u32 table = directory + 0x80;
    const u32 texture = directory + 0x100;
    image.set_word(directory, 0, directory + 0x40);
    image.set_word(directory, 4, 1);
    image.set_word(directory + 0x40, 0, 7);
    image.set_word(directory + 0x40, 4, table);
    image.set_word(table, 0, 1);
    image.set_word(table, 4, directory + 0xC0);
    image.set_word(directory + 0xC0, 0, texture);
    image.set_word(texture, 0, 0x1234);
    image.set_word(texture, 4, 0x30);
    image.set_half(texture, 8, 2);
    image.set_half(texture, 10, 3);
    image.set_byte(texture, 0xE, 30);
    image.set_byte(texture, 0xF, 1);
    image.set_byte(texture, 0x11, 2);
    image.set_byte(texture, 0x12, 9);
    image.set_word(texture, 0x14, 0x8000);
    image.set_word(texture, 0x18, 24);
    std::vector<std::pair<u32, u32>> registrations;
    RuntimeNodeRelocationBoundaries d3d;
    u32 headers = 0;
    d3d.set_texture_header = [&](u32 width, u32 height, u32 levels, u32 usage,
        u32 format, u32 pool, u32 resource, u32 address, u32 pitch)
    {
        require(width == 2 && height == 3 && levels == 1 && usage == 0 && format == 0x1234 &&
            pool == 0 && resource == texture + 0x30 + headers * 0x14 && address == 0 && pitch == 0,
            "texture header arguments");
        ++headers;
    };
    d3d.register_resource = [&](u32 resource, u32 address)
    {
        registrations.emplace_back(resource, address);
    };
    RuntimeSpecialTextures textures;
    postload_runtime_texture_table(image, table, directory, textures, d3d);
    require(headers == 2 && registrations == std::vector<std::pair<u32, u32>>{
        {texture + 0x30, 0x8000}, {texture + 0x44, 0x8018}} &&
        image.byte(texture, 0x12) == 0 && image.byte(texture, 0x13) == 255,
        "texture frame reset, multi-frame flag and frame data stride");
    image.set_byte(texture, 0x12, 1);
    require(lookup_runtime_model_texture(image, directory, 0) == texture + 0x44,
        "texture callback selects current frame");
    std::array<u8, 28> locked;
    locked.fill(0xAA);
    u32 locks = 0;
    d3d.lock_texture = [&](u32 resource, u32 level, u32 rectangle, u32 flags)
    {
        require(resource == texture + 0x44 && level == 0 && rectangle == 0 && flags == 0,
            "cleanup locks selected texture before clearing registration");
        ++locks;
        return std::span<u8>(locked);
    };
    unload_runtime_texture_table(image, table, textures, d3d);
    require(locks == 1 && locked[0] == 0 && locked[23] == 0 && locked[24] == 0xAA &&
        !textures[30].get_texture && textures[30].width == 0 && textures[30].format == 0x1234,
        "special cleanup clears exactly width*height DWORDs and retains format");
}

void test_directory_postload_header()
{
    std::array<u8, 2048> data{};
    const RuntimeRelocationImage image{data, 0x100000};
    const u32 base = image.address;
    image.set_word(base, 0, base + 0x40);
    image.set_word(base, 4, 4);
    image.set_word(base + 0x40, 0, 0);
    image.set_word(base + 0x40, 4, base + 0x100);
    image.set_word(base + 0x100, 0, base + 0x180);
    image.set_word(base + 0x48, 0, 0x12);
    image.set_word(base + 0x48, 4, base + 0x200);
    image.set_word(base + 0x50, 0, 0x12);
    image.set_word(base + 0x50, 4, base + 0x300);
    image.set_word(base + 0x58, 0, 0xC);
    image.set_word(base + 0x58, 4, base + 0x400);
    std::array<u8, 0x30> header;
    header.fill(0xAA);
    u32 freed = 0;
    RuntimeModelHeapBoundaries heap;
    heap.allocate = [&](u32 size)
    {
        require(size == 0x30, "runtime model header allocation size");
        return RuntimeModelAllocation{0x900000, header};
    };
    heap.free = [&](u32 address)
    {
        freed = address;
    };
    RuntimeSpecialTextures textures;
    const RuntimeNodeRelocationBoundaries d3d;
    const auto allocation = postload_runtime_model_directory(image, base, textures, heap, d3d);
    const RuntimeRelocationImage runtime{header, allocation.address};
    require(image.word(base, 0x10) == allocation.address &&
        runtime.word(allocation.address) == base + 0x200 &&
        runtime.word(allocation.address, 8) == base + 0x400 &&
        runtime.word(allocation.address, 0x24) == base + 0x180,
        "postload header stores first matching blocks and dereferenced tree");
    require(runtime.word(allocation.address, 0x20) == 0 &&
        runtime.word(allocation.address, 0x2C) == 0, "missing block fields initialized to zero");
    unload_runtime_model_directory(image, base, allocation, textures, heap, d3d);
    require(freed == 0x900000 && image.word(base, 0x10) == 0x900000,
        "unload frees header without clearing the original directory field");
}

void test_gpu_cache_release_order()
{
    std::array<u8, 0x2A0> data{};
    const RuntimeRelocationImage image{data, 0x100000};
    const u32 instance = image.address;
    const u32 first = instance + 4;
    const u32 second = first + 0xAC;
    image.set_word(first, 0, 10);
    image.set_word(first, 0x50, 2);
    image.set_word(first, 0x30, 20);
    image.set_word(first, 0x3C, 21);
    image.set_word(first, 0x5C, 30);
    image.set_word(first, 0x54, 31);
    image.set_word(first, 8, 32);
    image.set_word(first, 0x10, 40);
    image.set_word(first, 0x18, 42);
    image.set_word(second, 0, 50);
    image.set_word(second, 0x50, 0xFFFFFFFF);
    image.set_word(second, 0x30, 60);
    image.set_word(second, 0x8C, 70);
    std::vector<std::pair<char, u32>> calls;
    RuntimeModelHeapBoundaries heap;
    heap.free = [&](u32 address)
    {
        calls.emplace_back('h', address);
    };
    RuntimeNodeRelocationBoundaries d3d;
    d3d.release_resource = [&](u32 address)
    {
        calls.emplace_back('d', address);
    };
    release_runtime_model_gpu_cache(image, instance, heap, d3d);
    require(calls == std::vector<std::pair<char, u32>>{{'d', 10}, {'h', 20}, {'h', 21},
        {'h', 30}, {'h', 31}, {'h', 32}, {'d', 40}, {'d', 42}, {'d', 50}, {'h', 70}},
        "GPU cache release call order and optional-field order");
    require(image.word(first, 0x50) == 2 && image.word(first, 0x30) == 0 &&
        image.word(first, 0x5C) == 0 && image.word(second, 0x30) == 60,
        "cache cleanup retains counts and skips blocks for signed negative count");
    release_runtime_model_gpu_cache(image, instance, heap, d3d);
    require(calls.size() == 10, "cache cleanup repeated heap/D3D releases");
}

void test_relocated_asset_construction()
{
    std::array<u8, 4096> cpu{};
    const RuntimeRelocationImage image{cpu, 0x100000};
    const u32 base = image.address;
    const u32 tree = base + 0x100;
    image.set_word(base, 0, 0x40);
    image.set_word(base, 4, 1);
    image.set_word(base + 0x40, 0, 0);
    image.set_word(base + 0x40, 4, 0x80);
    image.set_word(base + 0x80, 0, 0x100);
    image.set_word(tree, 0, 2048);
    image.set_word(tree, 8, 0x40);
    image.set_word(tree, 0xC, 1);
    image.set_word(tree, 0x10, 0x80);
    image.set_word(tree + 0x40, 0, 0xC0);
    image.set_word(tree + 0x80, 0, 2);
    image.set_word(tree + 0x80, 4, 0xA0);
    image.set_word(tree + 0xA0, 0, 0x400);
    image.set_word(tree + 0xA0, 4, 0x450);
    image.set_word(tree + 0xAC, 0, 0x420);
    image.set_word(tree + 0xAC, 4, 0x454);
    image.set_word(tree + 0x450, 0, 0xC0);
    image.set_word(tree + 0x454, 0, 12);
    std::memcpy(image.pointer(tree + 0x400), "shader_plate", 13);
    std::memcpy(image.pointer(tree + 0x420), "MORPH_TARGET_BLINK", 18);
    std::memcpy(image.pointer(tree + 0x440), "ndShaderParam2", 15);
    std::memcpy(image.pointer(tree + 0x460), "ndSkeleton", 11);
    image.set_word(tree + 0xC0, 0, 0x440);
    image.set_word(tree + 0xC0, 0xC, 128);
    image.set_word(tree + 0xC0, 0x14, 0x160);
    image.set_word(tree + 0xC0, 0x20, 0x200);
    image.set_word(tree + 0x160, 0, 0x460);
    image.set_word(tree + 0x160, 0xC, 4);
    image.set_word(tree + 0x160, 0x20, 2);
    image.set_word(tree + 0x160, 0x24, 0x1A0);
    image.set_half(tree + 0x1A0, 0, 0xFFFF);
    image.set_half(tree + 0x1C0, 0, 0);
    image.set_word(tree + 0x1C0, 4, std::bit_cast<u32>(2.f));
    image.set_word(tree + 0x1C0, 0x10, std::bit_cast<u32>(3.f));
    image.set_word(tree + 0x200, 4, 0x280);
    image.set_word(tree + 0x200, 0x10, 1);
    image.set_word(tree + 0x200, 0x24, 0x300);
    image.set_word(tree + 0x200, 0x28, 2);
    image.set_word(tree + 0x200, 0x44, 8 + 0x44);
    image.set_word(tree + 0x300, 0, 0x340);
    image.set_word(tree + 0x300, 8, 0);
    image.set_word(tree + 0x310, 0, 0x360);
    image.set_word(tree + 0x310, 8, 10000);
    std::memcpy(image.pointer(tree + 0x340), "xf_plate", 9);
    std::memcpy(image.pointer(tree + 0x360), "xf_plate_du", 12);
    auto shader_cache = std::make_unique<RuntimeVertexShaderCache>();
    u32 default_shader = 0;
    const RuntimeNodeRelocationBoundaries d3d;
    const auto relocation_types = runtime_model_relocation_types(*shader_cache, default_shader, d3d);
    const RuntimeNodeRelocationTypes types{0x500000, relocation_types};
    std::array<u8, 0x30> header{};
    RuntimeModelHeapBoundaries heap;
    heap.allocate = [&](u32 size)
    {
        require(size == header.size(), "asset runtime header allocation");
        return RuntimeModelAllocation{0x900000, header};
    };
    u32 freed_header = 0;
    heap.free = [&](u32 address)
    {
        freed_header = address;
    };
    RuntimeSpecialTextures textures;
    const RuntimeBlendshapeBoundaries blendshape;
    const auto copy_types = runtime_model_copy_types(blendshape);
    RuntimeModelResourceViews views;
    const std::function<const u8*(u32)> gpu_pointer;
    RuntimeModelResourceLoader loader{{}, views, types, copy_types, textures, heap, d3d, gpu_pointer};
    loader.resources.emplace(cpu.data(), RuntimePreparedModelResource{image});
    AnimationResourceRegistry registry;
    registry.hooks[0].type = "anim";
    register_runtime_model_resource_hook(registry, loader);
    require(registry.hooks[0].type == std::string_view("anim") &&
        registry.hooks[1].type == std::string_view("model"), "model hook did not use the first free slot");
    std::array<AnimationBundleRecord, 1> bundle_records{{
        AnimationBundleRecord{"aid_model_fixture", 0, cpu, {}, {}, {}}}};
    std::array<AnimationBundlePool, 1> pools{{AnimationBundlePool{bundle_records, true}}};
    register_animation_resource_provider(registry,
        [&](u32, const AnimationResourceRecord& record, const void*& resolved)
        {
            return find_bundle_resource_provider(pools, record.name.c_str(), resolved);
        },
        [](u32, const void* resolved, AnimationLoadedResource& loaded)
        {
            return load_bundle_resource_provider(resolved, loaded);
        },
        [&](u32, const char* name, const AnimationLoadedResource&)
        {
            unload_bundle_resource_provider(pools, name);
        }, 3, nullptr);
    require(views.assets.empty(), "model view appeared before acquisition");
    // The retail bundle/executable format loader relocates its CPU allocation
    // before 0x00034AF0 exposes that allocation through the provider.
    AnimationLoadedResource prepared;
    prepared.cpu = cpu;
    registry.hooks[1].relocate(prepared);
    const auto acquired = acquire_animation_resource(registry, "aid_model_fixture");
    require(acquired.data() == cpu.data() && views.assets.size() == 1 &&
        registry.records[0].references == 1 && bundle_records[0].references == 1,
        "provider acquisition did not invoke the model postload hook");
    const auto& view = loader.resources.at(cpu.data()).view;
    require(view->asset.tree->roots[0]->type == 11 && view->asset.tree->roots[0]->child->type == 7 &&
        view->asset.tree->roots[0]->child->joints[1].parent == 0 &&
        view->asset.tree->roots[0]->child->joints[1].local.x == 2.f &&
        view->asset.tree->roots[0]->child->joints[1].world.x == 3.f,
        "relocated asset view lost node types or skeleton record offsets");
    RuntimeShaderRegistrationState registration;
    RuntimeShaderAnimationScratch scratch;
    auto model = make_runtime_model_instance(view->asset, 1, 1, copy_types, registration, scratch);
    require(model->primary != nullptr && model->primary->count == 2 &&
        model->shader_animations->count == 1 && model->shader_animations->entries[0]->entries[0].rates[0] == 1.f &&
        model->auxiliary_targets[0] == reinterpret_cast<float*>(model->tree_storage.bytes.data() + 12),
        "constructor did not consume decoded shader named-indirection or auxiliary value offset");
    release_runtime_model_instance(*model, registry, blendshape, heap, d3d);
    require(registry.records[0].references == 0 && views.assets.size() == 1,
        "model release bypassed separate resource lifetime");
    registry.hooks[1].unload(registry.loaded[0]);
    require(freed_header == 0x900000 && views.assets.empty() && !loader.resources.at(cpu.data()).view,
        "model unload hook did not release its header and decoded view");
}
}

int main()
{
    try
    {
        test_gpu_ranges();
        test_names_and_links();
        test_matrix_offsets();
        test_shader_interning();
        test_blendshape_registration();
        test_tree_dispatch();
        test_directory_pointer_bases();
        test_postload_ownership();
        test_texture_lifecycle();
        test_directory_postload_header();
        test_gpu_cache_release_order();
        test_relocated_asset_construction();
        std::cout << "Model relocation checks passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
