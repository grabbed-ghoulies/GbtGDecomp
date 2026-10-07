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
};


// DAT_00507810 stride 0x30. g_heap_current is DAT_00507808.
HeapSlot g_heap_slots[kHeapSlotCount];

i32 g_heap_current = 0;


i32 heap_index_for(void* block)
{
    // FUN_000f7100. EDI is the block. Four handles at DAT_00507834, stride 0x30.
    // msize (0x0014DED0) failure forces the footer byte to 0xFF, which cannot
    // match a heap id. The allocator at 0x000F72D0 writes that id at msize-2.
    // No match returns 3.
    for (i32 i = 0; i < kHeapSlotCount; ++i)
    {
        if (g_heap_slots[i].handle == nullptr || block == nullptr)
        {
            continue;
        }
        const u32 tag = 0xFFu;
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
        std::free(block);
    }
    g_heap_current = saved;
}

} // namespace gbtg
