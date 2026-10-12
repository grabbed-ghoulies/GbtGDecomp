#include "scene_resource_memory.h"
#include <limits>

namespace gbtg
{
RuntimeModelAllocation SceneResourceMemory::allocate(u32 size, u32 alignment)
{
    if (alignment == 0 || (alignment & (alignment - 1)) != 0)
    {
        throw Error("scene resource alignment must be a power of two");
    }
    const u64 address = (u64(next_) + alignment - 1) & ~u64(alignment - 1);
    const u64 end = address + std::max<u64>(size, 1);
    if (end > std::numeric_limits<u32>::max())
    {
        throw Error("scene resource address arena is exhausted");
    }
    auto& block = blocks_[u32(address)];
    block.assign(size, 0);
    next_ = u32((end + 15) & ~u64(15));
    return RuntimeModelAllocation{u32(address), block};
}

RuntimeModelAllocation SceneResourceMemory::copy(std::span<const u8> bytes, u32 alignment)
{
    const auto allocation = allocate(u32(bytes.size()), alignment);
    std::copy(bytes.begin(), bytes.end(), allocation.bytes.begin());
    return allocation;
}

void SceneResourceMemory::free(u32 address)
{
    if (blocks_.erase(address) == 0)
    {
        throw Error("scene resource free has no block at that address");
    }
}

std::span<u8> SceneResourceMemory::resolve(u32 address, size_t size) const
{
    auto block = blocks_.upper_bound(address);
    if (block == blocks_.begin())
    {
        throw Error("scene resource address is not mapped");
    }
    --block;
    const size_t offset = address - block->first;
    auto& bytes = const_cast<std::vector<u8>&>(block->second);
    if (offset > bytes.size() || size > bytes.size() - offset)
    {
        throw Error("scene resource range exceeds its block");
    }
    return std::span<u8>{bytes}.subspan(offset, size);
}

std::span<u8> SceneResourceMemory::tail(u32 address) const
{
    auto block = blocks_.upper_bound(address);
    if (block == blocks_.begin())
    {
        throw Error("scene resource address is not mapped");
    }
    --block;
    return resolve(address, block->second.size() - std::min<size_t>(address - block->first, block->second.size()));
}

u8* SceneResourceMemory::pointer(u32 address) const
{
    if (address == 0)
    {
        return nullptr;
    }
    return resolve(address, 0).data();
}

u32 SceneResourceMemory::address_of(const u8* pointer) const
{
    if (pointer == nullptr)
    {
        return 0;
    }
    for (const auto& [address, bytes] : blocks_)
    {
        if (pointer >= bytes.data() && pointer <= bytes.data() + bytes.size())
        {
            return address + u32(pointer - bytes.data());
        }
    }
    throw Error("native pointer is not in a scene resource block");
}

size_t SceneResourceMemory::block_count() const
{
    return blocks_.size();
}

RuntimeNodeRelocationBoundaries scene_d3d_boundaries(SceneD3DResources& d3d, SceneResourceMemory& memory)
{
    RuntimeNodeRelocationBoundaries boundaries;
    boundaries.register_resource = [&d3d](u32 resource, u32 data)
    {
        d3d.resources[resource].data = data;
    };
    // D3DXMatrixTranspose on the 4x4 float matrix at each address; the input may be the output.
    boundaries.transpose = [&memory](u32 output, u32 input)
    {
        float source[16];
        std::memcpy(source, memory.resolve(input, 64).data(), 64);
        float result[16];
        for (int row = 0; row < 4; ++row)
        {
            for (int column = 0; column < 4; ++column)
            {
                result[row * 4 + column] = source[column * 4 + row];
            }
        }
        std::memcpy(memory.resolve(output, 64).data(), result, 64);
    };
    boundaries.set_vertex_buffer_header = [&d3d](u32 length, u32 usage, u32 format, u32 pool, u32 resource,
        u32 data)
    {
        auto& record = d3d.resources[resource];
        record.kind = 1;
        record.header = {length, usage, format, pool, data, 0};
    };
    // The host keeps the shader source; assembly into Xbox microcode has no host counterpart.
    boundaries.assemble_shader = [&d3d](u32, u32 source, u32 length, u32, u32, u32& buffer, u32, u32, u32, u32, u32)
    {
        buffer = d3d.next_handle++;
        auto& data = d3d.assembled[buffer];
        data.resize(8);
        std::memcpy(data.data(), &source, 4);
        std::memcpy(data.data() + 4, &length, 4);
    };
    boundaries.assembled_shader_data = [&d3d](u32 buffer)
    {
        u32 source;
        std::memcpy(&source, d3d.assembled.at(buffer).data(), 4);
        return source;
    };
    boundaries.create_pixel_shader = [&d3d](u32 data, u32& handle)
    {
        handle = d3d.next_handle++;
        d3d.pixel_shaders[handle] = data;
    };
    boundaries.delete_pixel_shader = [&d3d](u32 handle)
    {
        d3d.pixel_shaders.erase(handle);
    };
    boundaries.set_push_buffer_header = [&d3d](u32 size, u32 usage, u32 resource, u32 data)
    {
        auto& record = d3d.resources[resource];
        record.kind = 2;
        record.header = {size, usage, data, 0, 0, 0};
    };
    // The dump passes zero data and pitch; 0x000534A0 registers the data separately.
    boundaries.set_texture_header = [&d3d](u32 width, u32 height, u32 levels, u32 usage, u32 format, u32 pool,
        u32 texture, u32, u32)
    {
        auto& record = d3d.resources[texture];
        record.kind = 3;
        record.header = {width, height, levels, usage, format, pool};
    };
    boundaries.lock_texture = [&d3d, &memory](u32 texture, u32, u32, u32)
    {
        return memory.tail(d3d.resources.at(texture).data);
    };
    boundaries.release_resource = [&d3d](u32 resource)
    {
        d3d.resources.erase(resource);
    };
    return boundaries;
}

RuntimeModelHeapBoundaries scene_model_heap_boundaries(SceneResourceMemory& memory)
{
    RuntimeModelHeapBoundaries heap;
    heap.allocate = [&memory](u32 size)
    {
        return memory.allocate(size, 0x10);
    };
    heap.allocate_physical = [&memory](u32 size, u32 alignment, const char*, u32)
    {
        return memory.allocate(size, alignment);
    };
    heap.free = [&memory](u32 address)
    {
        memory.free(address);
    };
    return heap;
}

RuntimeBlendshapeBoundaries scene_blendshape_boundaries(SceneResourceMemory& memory)
{
    RuntimeBlendshapeBoundaries boundaries;
    boundaries.allocator = [&memory](u32 size, u32 alignment, const char*, u32)
    {
        return memory.allocate(size, alignment).bytes.data();
    };
    // The host D3D keeps no header for a blendshape buffer; its data pointer is already in the record.
    boundaries.register_resource = [](RuntimeBlendshapeVertexBuffer&, const u8*)
    {
    };
    // The host GPU has no pending work on a buffer.
    boundaries.is_busy = [](RuntimeBlendshapeVertexBuffer&)
    {
        return 0;
    };
    boundaries.free_physical = [&memory](const u8* pointer, const char*, u32)
    {
        memory.free(memory.address_of(pointer));
    };
    return boundaries;
}
}
