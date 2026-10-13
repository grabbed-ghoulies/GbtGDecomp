#include "gbtg/entity.h"
#include "gbtg/anim.h"
#include "gbtg/model_runtime.h"
#include "gbtg/actor_runtime.h"
#include "gbtg/gfx.h"

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <malloc.h>
#include <span>
#include <vector>


#include "entity_types.h"
#include "entity_heap.h"
#include "aligned_heap.h"
#include <unordered_map>

namespace gbtg
{
using namespace entity_detail;

namespace entity_detail
{


constexpr i32 kHeapSlotCount = 4;


struct HeapSlot
{
    u32 id = 0;             // record + 0x20
    void* handle = nullptr; // record + 0x24 (DAT_00507834 for slot 0)
    u32 alignment = 0; // +0x28, temporarily changed by 0x000F7330.
};


// DAT_00507810 stride 0x30. g_heap_current is DAT_00507808.
HeapSlot g_heap_slots[kHeapSlotCount];

i32 g_heap_current = 0;

namespace
{
// Native HeapAlloc/HeapSize/HeapFree mapping for allocations whose retail heap
// record requests alignment. Their alignment metadata belongs to the host heap.
std::unordered_map<void*, u32> aligned_blocks;

size_t native_heap_size(void* block)
{
    const auto found = aligned_blocks.find(block);
    if (found != aligned_blocks.end())
    {
        return _aligned_msize(block, found->second, 0);
    }
    return _msize(block);
}

void native_heap_free(void* block)
{
    const auto found = aligned_blocks.find(block);
    if (found != aligned_blocks.end())
    {
        aligned_blocks.erase(found);
        _aligned_free(block);
        return;
    }
    std::free(block);
}
}


// Xbox 0x000F7100
i32 heap_index_for(void* block)
{
    // FUN_000f7100. EDI is the block. Four handles at DAT_00507834, stride 0x30.
    // msize (0x0014DED0) failure forces the footer byte to 0xFF, which cannot
    // match a heap id. The allocator at 0x000F72D0 writes that id at msize-2.
    // No match returns 3.
    for (i32 i = 0; i < kHeapSlotCount; ++i)
    {
        if (g_heap_slots[i].handle == nullptr)
        {
            continue;
        }
        const size_t size = block == nullptr ? size_t(-1) : native_heap_size(block);
        const u32 tag = size == size_t(-1) ? 0xFFu : *(static_cast<const u8*>(block) + size - 2);
        if (tag == g_heap_slots[i].id)
        {
            return i;
        }
    }
    return 3;
}

} // namespace entity_detail


// Xbox 0x000F72D0
void* engine_heap_allocate(i32 size)
{
    // Xbox 0x000F72D0
    // The size is the stack argument. DAT_00507808 selects the 0x30-byte record
    // at DAT_00507810. A zero handle at +0x24 nulls that record and the load of
    // +0x24 faults; 0x000663E0 releases that case on the process heap, so this
    // allocates there too. 0x0014E872 takes (handle, 0, size+4). On a non-null
    // block, 0x0014DED0's msize stores u16 0x1234 at msize-4 and the byte at
    // record+0x20 at msize-2.
    const i32 index = g_heap_current;
    void* block = std::malloc(static_cast<size_t>(static_cast<u32>(size) + 4u));
    if (block == nullptr)
    {
        return nullptr;
    }
    const size_t span = _msize(block);
    u8* tag = static_cast<u8*>(block) + span - 4;
    tag[0] = 0x34;
    tag[1] = 0x12;
    tag[2] = static_cast<u8>(g_heap_slots[index].id);
    return block;
}


// Xbox 0x000F7330
void* engine_heap_allocate_aligned(i32 size, u32 alignment)
{
    auto& slot = g_heap_slots[g_heap_current];
    const u32 previous = slot.alignment;
    slot.alignment = alignment;
    // HeapAlloc consumes the temporary alignment field in the retail heap.
    const size_t requested = size_t(u32(size) + 4u);
    void* block = alignment == 0 ? std::malloc(requested) : _aligned_malloc(requested, alignment);
    if (block != nullptr)
    {
        if (alignment != 0)
        {
            aligned_blocks.emplace(block, alignment);
        }
        const size_t span = native_heap_size(block);
        auto* footer = static_cast<u8*>(block) + span - 4;
        footer[0] = 0x34;
        footer[1] = 0x12;
        footer[2] = u8(slot.id);
    }
    slot.alignment = previous;
    return block;
}

// Xbox 0x000F73A0
void* engine_heap_reallocate(void* block, i32 size)
{
    const u8 heap_id = *(static_cast<const u8*>(block) + _msize(block) - 2);
    void* grown = std::realloc(block, static_cast<size_t>(u32(size) + 4u));
    if (grown != nullptr)
    {
        auto* footer = static_cast<u8*>(grown) + _msize(grown) - 4;
        footer[0] = 0x34;
        footer[1] = 0x12;
        footer[2] = heap_id;
    }
    return grown;
}

// Xbox 0x000663E0
void engine_heap_free(void* block)
{
    // Xbox 0x000663E0
    // EAX is the block. The owning slot is selected, DAT_00507808 is switched
    // to that index for HeapFree(handle, 0, block), then the previous index
    // is restored. A null slot faults on Xbox; the process heap releases it.
    const i32 index = heap_index_for(block);
    const i32 saved = g_heap_current;
    const void* handle = g_heap_slots[index].handle;
    g_heap_current = index;
    if (handle != nullptr || block != nullptr)
    {
        native_heap_free(block);
    }
    g_heap_current = saved;
}

} // namespace gbtg
