#pragma once
#include "gbtg/model_relocation.h"
#include "gbtg/model_runtime.h"
#include <functional>
#include <map>
#include <vector>

namespace gbtg
{
// Address-space adapter for the relocated resource formats. Retail places resource CPU blocks,
// GPU segments and heap blocks in one 32-bit space and stores absolute addresses inside them.
// The scene keeps native copies and gives each one a distinct address range.
class SceneResourceMemory
{
public:
    SceneResourceMemory() = default;
    SceneResourceMemory(const SceneResourceMemory&) = delete;
    SceneResourceMemory& operator=(const SceneResourceMemory&) = delete;
    RuntimeModelAllocation allocate(u32 size, u32 alignment); // Zero-filled.
    RuntimeModelAllocation copy(std::span<const u8> bytes, u32 alignment);
    void free(u32 address); // Address of a block start.
    std::span<u8> resolve(u32 address, size_t size) const;
    std::span<u8> tail(u32 address) const; // From the address to the end of its block.
    u32 block_start(u32 address) const; // Start of the block holding the address.
    u8* pointer(u32 address) const; // Null for address 0.
    u32 address_of(const u8* pointer) const; // 0 for null.
    u32 find_address(const u8* pointer) const; // 0 for a pointer outside every block.
    size_t block_count() const;
    // Told the host bytes of a block before free releases them, so caches keyed by those bytes can drop it.
    std::function<void(const u8* begin, size_t size)> on_free;

private:
    std::map<u32, std::vector<u8>> blocks_;
    u32 next_ = 0x40000000; // Native address arena, not an emulated heap implementation.
};

// Host D3D state behind the model relocation boundaries. The PC renderer keeps no Xbox D3D headers,
// so the boundaries record each resource here, keyed by the header address the dump passes.
struct SceneD3DResources
{
    struct Resource
    {
        u32 kind = 0; // 1 vertex buffer, 2 push buffer, 3 texture.
        u32 data = 0; // Registered data address.
        std::array<u32, 6> header{}; // Header arguments, in dump order.
    };
    std::map<u32, Resource> resources;
    std::map<u32, u32> pixel_shaders; // Handle to the assembled data address it was created from.
    std::map<u32, std::vector<u8>> assembled; // Assembly buffer handle to its data.
    u32 next_handle = 1;
};
RuntimeNodeRelocationBoundaries scene_d3d_boundaries(SceneD3DResources& d3d, SceneResourceMemory& memory);
RuntimeModelHeapBoundaries scene_model_heap_boundaries(SceneResourceMemory& memory);
RuntimeBlendshapeBoundaries scene_blendshape_boundaries(SceneResourceMemory& memory);
}
