#include "reference_names.h"
#include "../entities/entity_heap.h"
#include <bit>
#include <cstring>

namespace gbtg
{
// Xbox 0x00066430
RuntimeReferenceNames* allocate_runtime_reference_names(u32 bytes)
{
    auto* pool = static_cast<RuntimeReferenceNames*>(engine_heap_allocate(
        i32(sizeof(RuntimeReferenceNames) + 10 * sizeof(RuntimeReferenceName))));
    auto* strings = static_cast<char*>(engine_heap_allocate(std::bit_cast<i32>(bytes)));
    pool->limit = strings + bytes;
    pool->strings = strings;
    pool->cursor = strings;
    std::memset(strings, 0, bytes);
    pool->begin = reinterpret_cast<RuntimeReferenceName*>(pool + 1);
    pool->end = pool->begin + 10;
    for (auto* row = pool->begin; row < pool->end; ++row)
    {
        row->name = nullptr;
        row->references = 0;
        row->length = 0;
        row->hash = 0;
    }
    return pool;
}

// Xbox 0x000666F0
RuntimeReferenceName* find_runtime_reference_name(RuntimeReferenceNames& pool,
    const char* name)
{
    auto* row = pool.begin;
    while (row < pool.end)
    {
        if (row->name != nullptr)
        {
            const char* stored = row->name;
            const char* candidate = name;
            char first = *stored;
            char second = *candidate;
            if (first == second)
            {
                do
                {
                    ++stored;
                    ++candidate;
                    if (first == '\0')
                    {
                        return row;
                    }
                    first = *stored;
                    second = *candidate;
                } while (first == second);
            }
            if (first == second)
            {
                return row;
            }
        }
        ++row;
    }
    return nullptr;
}

// Xbox 0x00066530
i32 retain_runtime_reference_name(RuntimeReferenceNames*& owner, const char* name)
{
    auto* pool = owner;
    auto* row = find_runtime_reference_name(*pool, name);
    if (row == nullptr)
    {
        const char* end = name;
        char character;
        do
        {
            character = *end++;
        } while (character != '\0');
        const u32 length = u32(end - name);
        if (size_t(pool->limit - pool->cursor) <= length)
        {
            const auto old_address = reinterpret_cast<uintptr_t>(pool->strings);
            const size_t used = size_t(pool->cursor - pool->strings);
            const u32 added = length > 0x100 ? length : 0x100;
            const u32 capacity = u32(pool->limit - pool->strings) + added;
            auto* strings = static_cast<char*>(engine_heap_reallocate(pool->strings, std::bit_cast<i32>(capacity)));
            pool->cursor = strings + used;
            pool->limit = strings + capacity;
            pool->strings = strings;
            std::memset(pool->cursor, 0, size_t(pool->limit - pool->cursor));
            if (reinterpret_cast<uintptr_t>(strings) != old_address)
            {
                for (auto* entry = pool->begin; entry < pool->end; ++entry)
                {
                    if (entry->name != nullptr)
                    {
                        entry->name = strings + (reinterpret_cast<uintptr_t>(entry->name) - old_address);
                    }
                }
            }
        }
        row = pool->begin;
        auto* end_row = pool->end;
        while (row < end_row)
        {
            if (row->name == nullptr)
            {
                break;
            }
            ++row;
        }
        if (row == end_row)
        {
            const size_t count = size_t(end_row - pool->begin);
            const size_t capacity = count + 10;
            pool = static_cast<RuntimeReferenceNames*>(engine_heap_reallocate(pool,
                i32(sizeof(RuntimeReferenceNames) + capacity * sizeof(RuntimeReferenceName))));
            pool->begin = reinterpret_cast<RuntimeReferenceName*>(pool + 1);
            pool->end = pool->begin + capacity;
            row = pool->begin + count;
            for (auto* entry = row; entry < pool->end; ++entry)
            {
                entry->name = nullptr;
                entry->references = 0;
                entry->length = 0;
                entry->hash = 0;
            }
            owner = pool;
        }
        row->name = pool->cursor;
        row->length = u16(length);
        u32 hash = 0;
        u32 prefix;
        std::memcpy(&prefix, name, 4);
        if (prefix == 0x5F646961)
        {
            const u8* cursor = reinterpret_cast<const u8*>(name + 4);
            u8 byte = *cursor;
            while (byte != 0)
            {
                hash = (byte & 0xDF) + hash * 0x10;
                ++cursor;
                const u32 high = hash & 0xF0000000;
                if (high != 0)
                {
                    hash ^= (high >> 0x18) | high;
                }
                byte = *cursor;
            }
        }
        row->hash = hash;
        char* output = row->name;
        do
        {
            character = *name++;
            *output++ = character;
        } while (character != '\0');
        pool->cursor += length;
    }
    ++row->references;
    return i32(row - pool->begin);
}

// Xbox 0x000664A0
void release_runtime_reference_names(RuntimeReferenceNames* pool)
{
    engine_heap_free(pool->strings);
    engine_heap_free(pool);
}

// Xbox 0x00035760
void release_runtime_reference_name_owner(RuntimeReferenceNames** owner)
{
    release_runtime_reference_names(*owner);
    engine_heap_free(owner);
}
}
