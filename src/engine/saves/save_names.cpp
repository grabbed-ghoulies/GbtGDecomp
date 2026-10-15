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

namespace gbtg
{
namespace
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

// DAT_005462e4. +0 is the stride-0x18 slot pool. +8 is the stride-0x100
// name pool. +0x10 is the count 0x0010F2D0 increments. +0x14 is the float
// 0x00033BF2 decays by DAT_005096C0 while it stays above zero.
struct SaveScan
{
    SavePool* slots = nullptr;
    i32* slot_enumerator = nullptr; // +4, the 0x0010D710 / 0x0010D7A0 cursor.
    SavePool* names = nullptr;
    i32* enumerator = nullptr;
    i32 pending = 0;
    float timer = 0.f;
};

SaveScan* g_save_scan = nullptr;

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

// 0x0014C3AF. The file is UTF-16. The first wchar was already consumed by
// the 2-byte probe. The first "Name=" wins, including one sitting inside
// "TitleName=". The value runs until CR LF and is clipped to 0x7F wchars.
bool take_save_name(const std::vector<char16_t>& text, char16_t* dest)
{
    constexpr i32 kMax = 0x7F;
    for (size_t i = 0; i + 5 <= text.size(); ++i)
    {
        if (text[i] != u'N' || text[i + 1] != u'a' || text[i + 2] != u'm' || text[i + 3] != u'e' ||
            text[i + 4] != u'=')
        {
            continue;
        }
        const size_t value = i + 5;
        size_t end = value;
        while (end + 1 < text.size())
        {
            if (text[end] == u'\r' && text[end + 1] == u'\n')
            {
                size_t count = end - value;
                if (count > static_cast<size_t>(kMax))
                {
                    count = static_cast<size_t>(kMax);
                }
                for (size_t n = 0; n < count; ++n)
                {
                    dest[n] = text[value + n];
                }
                dest[count] = 0;
                return true;
            }
            end += 1;
        }
        return false;
    }
    return false;
}

// 0x0014C532. Directories only. Opens dirname\SaveMeta.xbx, requires a
// 2-byte read, then the Name field. A miss returns false so the walk
// continues. The wide value is what 0x0010F1C0 copies into the record.
bool read_save_meta_name(const std::filesystem::path& dir, char16_t* dest)
{
    const std::filesystem::path file = dir / "SaveMeta.xbx";
    std::ifstream in(file, std::ios::binary);
    if (!in)
    {
        return false;
    }
    unsigned char probe[2];
    in.read(reinterpret_cast<char*>(probe), 2);
    if (in.gcount() != 2)
    {
        return false;
    }
    std::vector<unsigned char> rest;
    rest.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    std::vector<char16_t> text(rest.size() / 2);
    for (size_t i = 0; i < text.size(); ++i)
    {
        text[i] = static_cast<char16_t>(rest[i * 2] | (static_cast<unsigned>(rest[i * 2 + 1]) << 8));
    }
    return take_save_name(text, dest);
}

// 0x0010F1C0. FindFirstFile of U:\*. Each directory whose SaveMeta.xbx
// yields a Name is one stride-0x100 record, copied with wcscpy. A missing
// drive returns -1 from the open and leaves the pool empty.
void scan_save_names(SavePool& names)
{
    std::error_code ec;
    const std::filesystem::directory_iterator end;
    std::filesystem::directory_iterator it(std::filesystem::path("U:\\"), ec);
    if (ec)
    {
        return;
    }
    for (; it != end; it.increment(ec))
    {
        if (ec)
        {
            break;
        }
        std::error_code dir_ec;
        if (!it->is_directory(dir_ec) || dir_ec)
        {
            continue;
        }
        char16_t name[0x80];
        name[0] = 0;
        if (!read_save_meta_name(it->path(), name))
        {
            continue;
        }
        u8* record = bump_save_record(names);
        if (record == nullptr)
        {
            return;
        }
        auto* dest = reinterpret_cast<char16_t*>(record);
        const char16_t* src = name;
        while ((*dest++ = *src++) != 0)
        {
        }
    }
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
        release_save_pool(g_save_scan->slots);
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
    scan->slots = nullptr;
    SavePool* slots = make_save_pool(0x18, 10);
    if (slots == nullptr)
    {
        delete scan;
        return;
    }
    scan->slots = slots;
    scan->names = nullptr;
    SavePool* names = make_save_pool(0x100, 10);
    if (names == nullptr)
    {
        release_save_pool(slots);
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
    SavePool& pool = *g_save_scan->slots;
    u8* record = pool.begin + pool.stride * *index;
    if (record != pool.cursor)
    {
        ++*index;
        if (record != nullptr)
        {
            throw Error("0x0010D710 copies a loaded save slot's name (+4 +2), which no translated code creates");
        }
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
    SavePool& pool = *g_save_scan->slots;
    u8* record = pool.begin + pool.stride * *index;
    if (record != pool.cursor)
    {
        ++*index;
        if (record != nullptr)
        {
            throw Error("0x0010D7A0 copies a loaded save slot's name (+4 +2), which no translated code creates");
        }
    }
    engine_heap_free(g_save_scan->slot_enumerator);
    g_save_scan->slot_enumerator = nullptr;
    *name = 0;
    return false;
}

// Xbox 0x0010F620, EBX the name: whether a loaded save slot (the stride-0x18 pool at DAT_005462E4 +0) holds it.
bool save_slot_loaded(const char16_t* name)
{
    SavePool& pool = *g_save_scan->slots;
    for (u8* record = pool.begin; record < pool.cursor; record += pool.stride)
    {
        (void)name;
        throw Error("0x0010F620 compares a loaded save slot (+4 +2), which no translated code creates");
    }
    return false;
}

} // namespace gbtg
