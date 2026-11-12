#pragma once
#include "gbtg/types.h"

#include <functional>

namespace gbtg
{
// The model library's callback globals. Boot 0x000F8740 fills them through the library's setters, and the model draw
// calls each one that is set.
struct RuntimeModelLibraryHooks
{
    std::function<void(u32 material)> material; // DAT_004D9750, 0x0015AD20 calls it after a material's states.
    std::function<void(u32 texture, u32 stage)> stage_texture; // DAT_004D9754, 0x0015AD20 calls it per texture stage.
    // DAT_004D9758: the address 0x0015B2F0 stores. No code reads it (a byte scan of the XBE finds only the store).
    u32 texture_frames = 0;
    std::function<void(u32 shader)> post_shader; // DAT_004D9768, 0x00157CE0 calls it after a shader's states.
    // DAT_00437098: the address 0x001565F0 stores. No code reads it (a byte scan of the XBE finds only the store).
    u32 texture_lock = 0;
};
// The model library's shader-animation allocator. Only 0x00165AB0 reads or writes the buffer words, and nothing else
// stores the buffer, so it is null whenever 0x00165AB0 runs.
struct RuntimeModelLibraryAllocator
{
    void* buffer = nullptr; // DAT_004DD0B8
    u32 unk_bc = 0; // DAT_004DD0BC
    u32 unk_c0 = 0; // DAT_004DD0C0
    // DAT_004DD0C4 and DAT_004DD0C8: the heap the shader animation lists (0x00165570, 0x00165830) are allocated from
    // and released to (0x00165A60). Those translations allocate with the host heap.
    std::function<void*(u32 size)> allocate;
    std::function<void(void* block)> release;
};
}
