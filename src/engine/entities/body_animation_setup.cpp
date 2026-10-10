#include "body_animation_setup.h"
#include "entity_heap.h"
#include <cstring>

namespace gbtg
{
// Xbox 0x00066140
RuntimeBodyBitStorage* allocate_runtime_body_bits(u32 count)
{
    const u32 bytes = (count + 7u) >> 3;
    auto* storage = static_cast<RuntimeBodyBitStorage*>(
        engine_heap_allocate(i32(u32(sizeof(RuntimeBodyBitStorage)) + bytes)));
    if (storage != nullptr)
    {
        storage->bit_count = count;
        storage->byte_count = bytes;
        storage->bytes = reinterpret_cast<u8*>(storage + 1);
        std::memset(storage->bytes, 0, bytes);
    }
    return storage;
}

// Xbox 0x0001B9E0
RuntimeBodyAnimationGroup* retain_runtime_body_animation_groups(const u8* resource,
    u32& count, std::span<RuntimeBodyResourceCacheEntry> cache, i32 cache_count)
{
    for (i32 i = 0; i < cache_count; ++i)
    {
        if (cache[i].second == resource)
        {
            ++cache[i].references;
            count = cache[i].first;
            return static_cast<RuntimeBodyAnimationGroup*>(cache[i].resource);
        }
    }
    return nullptr;
}

// Xbox 0x0001C400
GhoulySpawnPool* accumulate_runtime_body_sequence_length(GhoulySpawnPool* pool, u32 length)
{
    for (u8* row = pool->begin; row < pool->cursor; row += 4)
    {
        if (read_u16({row, 4}, 0) == length)
        {
            const u16 count = u16(read_u16({row, 4}, 2) + 1);
            std::memcpy(row + 2, &count, 2);
            return pool;
        }
    }
    if (pool->cursor == pool->limit)
    {
        const i32 used = i32(pool->cursor - pool->begin) / pool->stride;
        const i32 capacity = used + pool->grow;
        pool = static_cast<GhoulySpawnPool*>(engine_heap_reallocate(pool,
            i32(sizeof(GhoulySpawnPool)) + pool->stride * capacity));
        pool->begin = reinterpret_cast<u8*>(pool + 1);
        pool->cursor = pool->begin + pool->stride * used;
        pool->limit = pool->begin + pool->stride * capacity;
    }
    u8* row = pool->cursor;
    pool->cursor += pool->stride;
    const u16 stored_length = u16(length);
    const u16 one = 1;
    std::memcpy(row, &stored_length, 2);
    std::memcpy(row + 2, &one, 2);
    return pool;
}

// Xbox 0x0001C490
u32 runtime_body_animation_group_bytes(const GhoulySpawnPool& pool)
{
    u32 size = 0;
    for (const u8* row = pool.begin; row < pool.cursor; row += 4)
    {
        size += u32(sizeof(RuntimeBodyAnimationGroup))
            + u32(read_u16({row, 4}, 2)) * read_u16({row, 4}, 0) * u32(sizeof(const u8*));
    }
    return size;
}

// Xbox 0x0001C4C0
void initialize_runtime_body_animation_group_headers(RuntimeBodyAnimationGroup* groups,
    const GhoulySpawnPool& pool)
{
    const i32 count = i32(pool.cursor - pool.begin) / pool.stride;
    auto* values = reinterpret_cast<const u8**>(groups + count);
    size_t index = 0;
    for (const u8* row = pool.begin; row < pool.cursor; row += 4)
    {
        groups[index].length = read_u16({row, 4}, 0);
        groups[index].values = values;
        groups[index].count = 0;
        values += u32(read_u16({row, 4}, 2)) * groups[index].length;
        ++index;
    }
}

// Xbox 0x0001C220
void build_runtime_body_animation_groups(RuntimeBodyAnimationSetup& context)
{
    u32 cached_count;
    context.groups = retain_runtime_body_animation_groups(context.resource.data(), cached_count,
        context.cache, context.cache_count);
    if (context.groups != nullptr)
    {
        context.group_count = u8(cached_count);
        return;
    }
    const auto resource = context.resource;
    const u8* sequence = context.sequences.at(resource.data());
    const u8* end = sequence + size_t(read_u32(resource, 0xC)) * 2;
    auto* pool = static_cast<GhoulySpawnPool*>(engine_heap_allocate(i32(sizeof(GhoulySpawnPool)) + 32));
    pool->stride = 4;
    pool->grow = 8;
    pool->begin = reinterpret_cast<u8*>(pool + 1);
    pool->cursor = pool->begin;
    pool->limit = pool->begin + 32;
    i32 maximum = 0;
    while (read_u16({sequence, 2}, 0) != 0)
    {
        const u8* cursor = sequence;
        i32 length = 0;
        do
        {
            if (cursor >= end)
            {
                break;
            }
            cursor += 2;
            ++length;
        } while (read_u16({cursor, 2}, 0) != 0);
        if (length == 0)
        {
            break;
        }
        sequence += size_t(length + 1) * 2;
        pool = accumulate_runtime_body_sequence_length(pool, u32(length));
        if (maximum < length)
        {
            maximum = length;
        }
    }
    if (maximum == 0)
    {
        engine_heap_free(pool);
        return;
    }
    const i32 count = i32(pool->cursor - pool->begin) / pool->stride;
    context.groups = static_cast<RuntimeBodyAnimationGroup*>(
        engine_heap_allocate(i32(runtime_body_animation_group_bytes(*pool))));
    context.group_count = u8(count);
    initialize_runtime_body_animation_group_headers(context.groups, *pool);
    engine_heap_free(pool);
    for (i32 index = 0; index < count; ++index)
    {
        sequence = context.sequences.at(resource.data());
        end = sequence + size_t(read_u32(resource, 0xC)) * 2;
        auto& group = context.groups[index];
        while (read_u16({sequence, 2}, 0) != 0)
        {
            const u8* cursor = sequence;
            u32 length = 0;
            do
            {
                if (cursor >= end)
                {
                    break;
                }
                cursor += 2;
                ++length;
            } while (read_u16({cursor, 2}, 0) != 0);
            if (length == 0)
            {
                break;
            }
            if (length == group.length)
            {
                const u32 offset = u32(group.count) * group.length;
                for (u32 i = 0; i < group.length; ++i)
                {
                    group.values[offset + i] = sequence + i * 2;
                }
                ++group.count;
            }
            sequence += size_t(length + 1) * 2;
        }
    }
    const i32 slot = context.cache_count++;
    context.cache[slot].second = context.resource.data();
    context.cache[slot].resource = context.groups;
    context.cache[slot].first = context.group_count;
    context.cache[slot].references = 1;
}

// Xbox 0x0001C1C0
u32 maximum_runtime_body_animation_count(const RuntimeBodyAnimationSetup& context)
{
    u32 count = 0;
    if (context.resource.data() != nullptr)
    {
        const u16 first = read_u16(context.resource, 4);
        if (first != 0)
        {
            count = first;
        }
        const u16 second = read_u16(context.resource, 6);
        if (second != 0 && count <= second)
        {
            count = second;
        }
        if (context.groups != nullptr && context.group_count != 0)
        {
            for (u32 i = 0; i < context.group_count; ++i)
            {
                if (count <= context.groups[i].count)
                {
                    count = context.groups[i].count;
                }
            }
        }
    }
    return count;
}

// Xbox 0x0001CB60
void acquire_runtime_body_animation(std::span<const u8> parameters, RuntimeBodyAnimationSetup& context)
{
    if (read_u32(parameters, 8) == 0x5F646961)
    {
        const auto acquired = acquire_animation_resource(context.registry,
            reinterpret_cast<const char*>(parameters.data() + 8));
        if (acquired.data() == nullptr)
        {
            resource_error(context.error, 2);
        }
        context.resource = acquired;
    }
    if (context.resource.data() != nullptr && read_u32(context.resource, 0x10) != 0)
    {
        build_runtime_body_animation_groups(context);
    }
    if (context.resource.data() != nullptr)
    {
        u32 count = maximum_runtime_body_animation_count(context);
        for (const size_t offset : {size_t(0x1C), size_t(0x24), size_t(0x14)})
        {
            const u16 value = read_u16(context.resource, offset);
            if (i32(count) <= i32(value))
            {
                count = value;
            }
        }
        if (count != 0)
        {
            context.bits = allocate_runtime_body_bits(count);
        }
    }
}

// Xbox 0x0001D360
void initialize_runtime_body_animation_settings(std::span<u8> body,
    std::span<const u8> parameters, std::span<const u8> actor,
    std::span<const u8> class_rows, i32 class_count, RuntimeBodyAnimationSetup& context)
{
    acquire_runtime_body_animation(parameters, context);
    std::memcpy(body.data() + 0x78, parameters.data() + 0x198, 4);
    std::memcpy(body.data() + 0x7C, parameters.data() + 0x19C, 4);
    std::memcpy(body.data() + 0x80, parameters.data() + 0x1A0, 4);
    std::memcpy(body.data() + 0x6E, parameters.data() + 0x1A4, 2);
    for (i32 i = 0; i < class_count; ++i)
    {
        if (read_u16(class_rows, size_t(i) * 8) == read_u16(actor, 6))
        {
            std::memcpy(body.data() + 0x70, class_rows.data() + size_t(i) * 8 + 4, 4);
            return;
        }
    }
}

// Xbox 0x00051CA0
u32 count_runtime_body_model_records(const u8* data, u16 kind)
{
    const u8* end = data + 4 + size_t(read_u32({data, 4}, 0)) * 0x34;
    u32 count = 0;
    const u8* row = data + 4;
    while (row < end)
    {
        if (read_u16({row, 2}, 0) == kind)
        {
            ++count;
        }
        row += 0x34;
    }
    return count;
}

// Xbox 0x00040980
u32 count_runtime_body_model_kind(const RuntimeBodyModelNodes* model, u16 kind)
{
    if (model != nullptr)
    {
        for (const auto& node : model->nodes)
        {
            if (node.kind == 2)
            {
                if (node.data == nullptr)
                {
                    return 0;
                }
                return count_runtime_body_model_records(node.data, kind);
            }
        }
    }
    return 0;
}
}
