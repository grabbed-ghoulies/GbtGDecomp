#include "gbtg/model_relocation.hpp"
#include <bit>

namespace gbtg
{
namespace
{

// The repeated first-match loops in 0x00052B70 and 0x00052E50.
u32 find_block(const RuntimeRelocationImage& image, u32 directory, u32 type)
{
    u32 index = 0;
    if (image.word(directory, 4) != 0)
    {
        u32 entry = image.word(directory);
        do
        {
            if (image.word(entry) == type)
            {
                return image.word(image.word(directory) + index * 8, 4);
            }
            ++index;
            entry += 8;
        } while (index < image.word(directory, 4));
    }
    return 0;
}

}

// Xbox 0x00052B70. The runtime header is a distinct heap allocation.
RuntimeModelAllocation postload_runtime_model_directory(const RuntimeRelocationImage& image,
    u32 directory, RuntimeSpecialTextures& textures, const RuntimeModelHeapBoundaries& heap,
    const RuntimeNodeRelocationBoundaries& d3d)
{
    const auto allocation = heap.allocate(0x30);
    const RuntimeRelocationImage runtime{allocation.bytes, allocation.address};
    image.set_word(directory, 0x10, allocation.address);
    runtime.set_word(allocation.address, 0, find_block(image, directory, 0x12));
    runtime.set_word(allocation.address, 0x20, find_block(image, directory, 10));
    runtime.set_word(allocation.address, 0xC, find_block(image, directory, 0xD));
    runtime.set_word(allocation.address, 0x1C, find_block(image, directory, 0x15));
    runtime.set_word(allocation.address, 0x18, find_block(image, directory, 0x10));
    runtime.set_word(allocation.address, 0x10, find_block(image, directory, 0x11));
    runtime.set_word(allocation.address, 0x14, find_block(image, directory, 0xE));
    runtime.set_word(allocation.address, 4, find_block(image, directory, 6));
    runtime.set_word(allocation.address, 8, find_block(image, directory, 0xC));
    runtime.set_word(allocation.address, 0x24, image.word(find_block(image, directory, 0)));
    runtime.set_word(allocation.address, 0x28, find_block(image, directory, 7));
    runtime.set_word(allocation.address, 0x2C, find_block(image, directory, 0x18));
    postload_runtime_model_tree(image, runtime.word(allocation.address, 0x24), heap, d3d);
    const u32 textures_block = find_block(image, directory, 7);
    if (textures_block != 0)
    {
        postload_runtime_texture_table(image, textures_block, directory, textures, d3d);
    }
    if (runtime.word(allocation.address, 8) != 0)
    {
        postload_runtime_texture_table(image, runtime.word(allocation.address, 8) + 8,
            directory, textures, d3d);
    }
    if (runtime.word(allocation.address, 0xC) != 0)
    {
        postload_runtime_texture_table(image, runtime.word(allocation.address, 0xC) + 8,
            directory, textures, d3d);
    }
    return allocation;
}

// Xbox 0x00052E50. Heap selection and HeapFree are supplied by the heap boundary.
void unload_runtime_model_directory(const RuntimeRelocationImage& image, u32 directory,
    const RuntimeModelAllocation& allocation, RuntimeSpecialTextures& textures,
    const RuntimeModelHeapBoundaries& heap, const RuntimeNodeRelocationBoundaries& d3d)
{
    const RuntimeRelocationImage runtime{allocation.bytes, allocation.address};
    unload_runtime_model_tree(image, runtime.word(allocation.address, 0x24), heap, d3d);
    const u32 textures_block = find_block(image, directory, 7);
    if (textures_block != 0)
    {
        unload_runtime_texture_table(image, textures_block, textures, d3d);
    }
    if (runtime.word(allocation.address, 8) != 0)
    {
        unload_runtime_texture_table(image, runtime.word(allocation.address, 8) + 8, textures, d3d);
    }
    if (runtime.word(allocation.address, 0xC) != 0)
    {
        unload_runtime_texture_table(image, runtime.word(allocation.address, 0xC) + 8, textures, d3d);
    }
    heap.free(allocation.address);
}

// Xbox 0x000F9C20. Both cache channels occupy 0xAC bytes starting at +4.
void release_runtime_model_gpu_cache(const RuntimeRelocationImage& image, u32 instance,
    const RuntimeModelHeapBoundaries& heap, const RuntimeNodeRelocationBoundaries& d3d)
{
    u32 channel = instance + 4;
    i32 remaining = 2;
    do
    {
        if (image.word(channel) != 0)
        {
            d3d.release_resource(image.word(channel));
            image.set_word(channel, 0, 0);
        }
        if (channel != 0xFFFFFFD4 && std::bit_cast<i32>(image.word(channel, 0x50)) > 0)
        {
            i32 index = 0;
            u32 record = channel + 0x30;
            do
            {
                if (image.word(record) != 0)
                {
                    heap.free(image.word(record));
                    image.set_word(record, 0, 0);
                }
                ++index;
                record += 0xC;
            } while (index < std::bit_cast<i32>(image.word(channel, 0x50)));
        }
        // These are the nine consecutive optional-free branches in the dump,
        // in their original order; the last field is deliberately +8.
        for (const u32 offset : {0x5CU, 0x54U, 0x64U, 0x6CU, 0x74U, 0x7CU, 0x84U, 0x8CU, 8U})
        {
            if (image.word(channel, offset) != 0)
            {
                heap.free(image.word(channel, offset));
                image.set_word(channel, offset, 0);
            }
        }
        u32 resource = channel + 0x10;
        i32 resources = 3;
        do
        {
            if (image.word(resource) != 0)
            {
                d3d.release_resource(image.word(resource));
                image.set_word(resource, 0, 0);
            }
            resource += 4;
            --resources;
        } while (resources != 0);
        channel += 0xAC;
        --remaining;
    } while (remaining != 0);
}

} // namespace gbtg
