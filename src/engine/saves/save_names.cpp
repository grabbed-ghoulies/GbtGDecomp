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

#include "save_scan.h"
#include "xdk_saves.h"

namespace gbtg
{
// Header of a record pool. Retail stores this in a 0x10 byte block from
// 0x000F72D0: stride, grow, then begin / cursor / limit. An empty pool
// points all three one past the header. The port keeps those cursors
// here; the block still carries the two words 0x000F73A0 copies on grow.
struct SavePool
{
    u16 stride = 0;
    u16 grow = 0;
    u8* begin = nullptr;
    u8* cursor = nullptr;
    u8* limit = nullptr;
    u8* block = nullptr;
    i32 block_bytes = 0;
};

SaveScan* g_save_scan = nullptr;

namespace
{


void bind_save_pool(SavePool& pool, u8* block, i32 block_bytes, i32 used)
{
    pool.block = block;
    pool.block_bytes = block_bytes;
    std::memcpy(block, &pool.stride, sizeof(pool.stride));
    std::memcpy(block + 2, &pool.grow, sizeof(pool.grow));
    pool.begin = block + 0x10;
    const i32 stride = pool.stride == 0 ? 1 : static_cast<i32>(pool.stride);
    pool.cursor = pool.begin + static_cast<ptrdiff_t>(stride) * used;
    const i32 capacity = (block_bytes - 0x10) / stride;
    pool.limit = pool.begin + static_cast<ptrdiff_t>(stride) * capacity;
}

SavePool* make_save_pool(u16 stride, u16 grow)
{
    u8* block = static_cast<u8*>(engine_heap_allocate(0x10));
    if (block == nullptr)
    {
        return nullptr;
    }
    auto* pool = new SavePool;
    pool->stride = stride;
    pool->grow = grow;
    bind_save_pool(*pool, block, 0x10, 0);
    return pool;
}

void release_save_pool(SavePool* pool)
{
    if (pool == nullptr)
    {
        return;
    }
    if (pool->block != nullptr)
    {
        engine_heap_free(pool->block);
    }
    delete pool;
}

// 0x000662A0 via 0x000F73A0. used = (cursor - begin) / stride. The new
// count is used plus the grow word. The cursor stays on the same record.
void grow_save_pool(SavePool& pool)
{
    const i32 stride = pool.stride == 0 ? 1 : static_cast<i32>(pool.stride);
    i32 used = 0;
    if (pool.begin != nullptr && pool.cursor != nullptr && pool.cursor >= pool.begin)
    {
        used = static_cast<i32>(pool.cursor - pool.begin) / stride;
    }
    const i32 new_count = used + static_cast<i32>(pool.grow);
    const i32 new_bytes = stride * new_count + 0x10;
    u8* fresh = static_cast<u8*>(engine_heap_allocate(new_bytes));
    if (fresh == nullptr)
    {
        return;
    }
    if (pool.block != nullptr && pool.block_bytes > 0)
    {
        const i32 copy = pool.block_bytes < new_bytes ? pool.block_bytes : new_bytes;
        std::memcpy(fresh, pool.block, static_cast<size_t>(copy));
        engine_heap_free(pool.block);
    }
    bind_save_pool(pool, fresh, new_bytes, used);
}

u8* bump_save_record(SavePool& pool)
{
    if (pool.cursor == pool.limit)
    {
        grow_save_pool(pool);
    }
    if (pool.cursor == nullptr || pool.limit == nullptr || pool.cursor >= pool.limit)
    {
        return nullptr;
    }
    u8* record = pool.cursor;
    pool.cursor = record + pool.stride;
    std::memset(record, 0, pool.stride);
    return record;
}

// Xbox 0x0010F1C0: every save game on U:\ (0x0032A3C8) adds its name to the stride-0x100 pool.
void scan_save_names(SavePool& names)
{
    XGameFindData data;
    XSaveGameFind* find = find_first_xbox_save_game("U:\\", data); // XFindFirstSaveGame
    if (find == nullptr)
    {
        return;
    }
    do
    {
        u8* record = bump_save_record(names); // 0x000F73A0 grows a full pool.
        auto* dest = reinterpret_cast<char16_t*>(record);
        const char16_t* src = data.name();
        while ((*dest++ = *src++) != 0) // lstrcpyW
        {
        }
    }
    while (find_next_xbox_save_game(*find, data)); // XFindNextSaveGame
    close_xbox_save_game_find(find); // XFindClose
}

} // namespace

// Xbox 0x0010D3B0
void save_initialize_name_scan()
{
    // Xbox 0x0010D3B0
    // 0x000F72D0 allocates the 0x18 byte scan, then two 0x10 pool headers.
    // The first stride is 0x18 and the second is 0x100. Both grow words are
    // 10, and both cursors start empty. +4, +0xc, +0x10, and the float at
    // +0x14 are cleared, then 0x0010F1C0 fills the wide pool from U:\.
    // A second call drops the previous pools before the new allocations.
    if (g_save_scan != nullptr)
    {
        release_save_pool(g_save_scan->names);
        if (g_save_scan->enumerator != nullptr)
        {
            engine_heap_free(g_save_scan->enumerator);
        }
        if (g_save_scan->slot_enumerator != nullptr)
        {
            engine_heap_free(g_save_scan->slot_enumerator);
        }
        delete g_save_scan;
        g_save_scan = nullptr;
    }

    auto* scan = new SaveScan;
    scan->slots.reserve(10); // The stride-0x18 pool, grow 10.
    SavePool* names = make_save_pool(0x100, 10);
    if (names == nullptr)
    {
        delete scan;
        return;
    }
    scan->names = names;
    scan->slot_enumerator = nullptr;
    scan->enumerator = nullptr;
    scan->pending = 0;
    scan->timer = 0.f;
    g_save_scan = scan;
    scan_save_names(*names);
}

// Xbox 0x0010D810.
bool save_first_name(char16_t* name)
{
    if (g_save_scan->enumerator != nullptr)
    {
        engine_heap_free(g_save_scan->enumerator);
        g_save_scan->enumerator = nullptr;
    }
    auto* index = static_cast<i32*>(engine_heap_allocate(4));
    *index = 0;
    g_save_scan->enumerator = index;
    SavePool& pool = *g_save_scan->names;
    u8* record = pool.begin + pool.stride * *index;
    if (record != pool.cursor)
    {
        ++*index;
        if (record != nullptr)
        {
            const auto* source = reinterpret_cast<const char16_t*>(record);
            while ((*name++ = *source++) != 0)
            {
            }
            return true;
        }
    }
    engine_heap_free(g_save_scan->enumerator);
    g_save_scan->enumerator = nullptr;
    *name = 0;
    return false;
}

// Xbox 0x0010D8A0.
bool save_next_name(char16_t* name)
{
    i32* index = g_save_scan->enumerator;
    if (index == nullptr)
    {
        *name = 0;
        return false;
    }
    SavePool& pool = *g_save_scan->names;
    u8* record = pool.begin + pool.stride * *index;
    if (record != pool.cursor)
    {
        ++*index;
        if (record != nullptr)
        {
            const auto* source = reinterpret_cast<const char16_t*>(record);
            while ((*name++ = *source++) != 0)
            {
            }
            return true;
        }
    }
    engine_heap_free(g_save_scan->enumerator);
    g_save_scan->enumerator = nullptr;
    *name = 0;
    return false;
}


// Xbox 0x000663B0 with the predicate 0x0010E9B0 (lstrcmpW == 0), then 0x00066380 (or its inline copy): the
// name's record leaves the stride-0x100 name pool, the records after it moving down one stride.
void remove_save_name(const char16_t* name)
{
    SavePool& pool = *g_save_scan->names;
    for (u8* record = pool.begin; record < pool.cursor; record += pool.stride)
    {
        if (std::u16string_view(reinterpret_cast<const char16_t*>(record)) != name)
        {
            continue;
        }
        u8* last = pool.cursor - pool.stride;
        if (record < last)
        {
            std::memmove(record, record + pool.stride, size_t(last - record));
        }
        pool.cursor -= pool.stride;
        return;
    }
}

// Xbox 0x0010F290: the name added to the stride-0x100 name pool (grown when full).
void save_add_name(const char16_t* name)
{
    u8* record = bump_save_record(*g_save_scan->names);
    auto* dest = reinterpret_cast<char16_t*>(record);
    while ((*dest++ = *name++) != 0) // lstrcpyW
    {
    }
}

// Xbox 0x0010D710
bool save_first_slot_name(char16_t* name)
{
    if (g_save_scan->slot_enumerator != nullptr)
    {
        engine_heap_free(g_save_scan->slot_enumerator); // 0x000663E0
        g_save_scan->slot_enumerator = nullptr;
    }
    auto* index = static_cast<i32*>(engine_heap_allocate(4));
    *index = 0;
    g_save_scan->slot_enumerator = index;
    auto& slots = g_save_scan->slots;
    if (size_t(*index) != slots.size())
    {
        const SaveSlot& slot = slots[size_t(*index)];
        ++*index;
        const char16_t* source = slot.save->name(); // lstrcpyW(name, +4 +2)
        while ((*name++ = *source++) != 0)
        {
        }
        return true;
    }
    engine_heap_free(g_save_scan->slot_enumerator);
    g_save_scan->slot_enumerator = nullptr;
    *name = 0;
    return false;
}

// Xbox 0x0010D7A0, EBX the name
bool save_next_slot_name(char16_t* name)
{
    i32* index = g_save_scan->slot_enumerator;
    if (index == nullptr)
    {
        *name = 0;
        return false;
    }
    auto& slots = g_save_scan->slots;
    if (size_t(*index) != slots.size())
    {
        const SaveSlot& slot = slots[size_t(*index)];
        ++*index;
        const char16_t* source = slot.save->name();
        while ((*name++ = *source++) != 0)
        {
        }
        return true;
    }
    engine_heap_free(g_save_scan->slot_enumerator);
    g_save_scan->slot_enumerator = nullptr;
    *name = 0;
    return false;
}

// Xbox 0x0010F620, EBX the name: whether a loaded save holds it.
bool save_slot_loaded(const char16_t* name)
{
    for (const SaveSlot& slot : g_save_scan->slots)
    {
        if (std::u16string_view(slot.save->name()) == name) // lstrcmpW
        {
            return true;
        }
    }
    return false;
}

} // namespace gbtg
