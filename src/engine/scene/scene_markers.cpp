#include "scene_markers.h"
#include "../entities/entity_heap.h"
#include "gbtg/anim.h"
#include <bit>
#include <vector>
#include <cstdio>
#include <cstring>
#if defined(GBTG_INSPECTOR_APP)
#include <memory>
#endif

namespace gbtg
{
namespace
{
u32 word_at(const u8* bytes, size_t offset)
{
    u32 value;
    std::memcpy(&value, bytes + offset, 4);
    return value;
}

u16 half_at(const u8* bytes, size_t offset)
{
    u16 value;
    std::memcpy(&value, bytes + offset, 2);
    return value;
}

void put_word(u8* bytes, size_t offset, u32 value)
{
    std::memcpy(bytes + offset, &value, 4);
}

void put_half(u8* bytes, size_t offset, u16 value)
{
    std::memcpy(bytes + offset, &value, 2);
}

// The six position/rotation dwords (+0xC..+0x20) and the next-id word (+8) two rows exchange.
void exchange_marker_rows(u8* first, u8* second)
{
    u32 words[6];
    std::memcpy(words, first + 0xC, sizeof(words));
    const u16 next = half_at(first, 8);
    std::memcpy(first + 0xC, second + 0xC, sizeof(words));
    put_half(first, 8, half_at(second, 8));
    std::memcpy(second + 0xC, words, sizeof(words));
    put_half(second, 8, next);
}

u8* find_marker_row(u8* records, i16 id)
{
    const u32 count = word_at(records, 0);
    u8* row = records + 8;
    for (u32 index = 0; index < count; ++index)
    {
        if (i16(half_at(row, 6)) == id)
        {
            return row;
        }
        row += word_at(row, 0);
    }
    return nullptr;
}

// Next row of the block, or null past its used size.
u8* following_row(const u8* records, u8* row)
{
    u8* next = row + word_at(row, 0);
    return u32(next - records) < word_at(records, 4) ? next : nullptr;
}
}

// Xbox 0x000FFE20
u8* load_runtime_marker_resource(const char* name, AnimationResourceRegistry& registry, ResourceErrorState& error)
{
    i32 total = 8;
    auto* block = static_cast<u8*>(engine_heap_allocate(8));
    put_word(block, 0, 0);
    if (word_at(reinterpret_cast<const u8*>(name), 0) == 0x5F646961)
    {
        auto resource = acquire_animation_resource(registry, name);
        if (resource.data() == nullptr)
        {
            resource_error(error, 2);
            put_word(block, 4, u32(total));
            return block;
        }
        size_t row = 0;
        while (read_u16(resource, row + 4) != 0)
        {
            const u32 size = read_u32(resource, row);
            block = static_cast<u8*>(engine_heap_reallocate(block, i32(size) + total));
            std::memcpy(block + total, resource.data() + row, size);
            total += i32(size);
            put_word(block, 0, word_at(block, 0) + 1);
            row += size;
        }
        release_animation_resource(registry, resource);
    }
    put_word(block, 4, u32(total));
    return block;
}

// Xbox 0x0011CB80
u8* load_runtime_scene_markers(const char* scene_record, AnimationResourceRegistry& registry, ResourceErrorState& error)
{
    size_t index = 5;
    if (scene_record[5] != '_')
    {
        char value;
        do
        {
            value = scene_record[index + 1];
            ++index;
        } while (value != '_');
    }
    char name[0x80];
    std::snprintf(name, sizeof(name), "aid_%s_%s", "marker", scene_record + index + 1);
    return load_runtime_marker_resource(name, registry, error);
}

// Xbox 0x001401B0
void swap_runtime_marker_pairs(u8* records, const RuntimeSceneMarkerRandomizer& context)
{
    for (i32 index = 0; index < context.swap_count; ++index)
    {
        u8* first = find_marker_row(records, i16(context.swap_ids[size_t(index) * 2]));
        u8* second = find_marker_row(records, i16(context.swap_ids[size_t(index) * 2 + 1]));
        if (first != nullptr && second != nullptr)
        {
            exchange_marker_rows(first, second);
        }
    }
}

// Xbox 0x00140450
void shuffle_runtime_marker_rows(std::span<u8*> rows, u32& random)
{
    const i32 count = i32(rows.size());
    for (i32 index = 0; index < count; ++index)
    {
        i32 other;
        do
        {
            random = random * 0x19660D + 0x3C6EF35F;
            other = i32((std::bit_cast<float>((random & 0x7FFFFF) | 0x3F800000) - 1.f) * float(count));
        } while (other == index);
        exchange_marker_rows(rows[size_t(index)], rows[size_t(other)]);
    }
}

// Xbox 0x001402B0
void shuffle_runtime_marker_groups(u8* records, const RuntimeSceneMarkerRandomizer& context)
{
    if (context.group_count == 0)
    {
        return;
    }
    // The retail row pool is a stride-4 heap pool grown four entries at a time and reset per group.
    std::vector<u8*> pool;
    for (i32 group = 0; group < context.group_count; ++group)
    {
        i32 found = 0;
        for (size_t slot = 0; slot < 10; ++slot)
        {
            const u16 id = context.group_ids[size_t(group) * 10 + slot];
            if (id == 0)
            {
                continue;
            }
            const u32 count = word_at(records, 0);
            u8* row = records + 8;
            for (u32 index = 0; index < count; ++index)
            {
                if (half_at(row, 6) == id)
                {
                    pool.push_back(row);
                    ++found;
                    break;
                }
                row += word_at(row, 0);
            }
        }
        if (found > 1)
        {
            shuffle_runtime_marker_rows(pool, context.random);
        }
        pool.clear();
    }
}

// Xbox 0x0011EEC0
void dispatch_runtime_marker_records(u8* records, const RuntimeSceneMarkerHandlers& handlers)
{
    // DAT_00312098: the 27 kinds in dispatch order.
    static constexpr std::array<u16, 0x1B> order{8, 0x14, 4, 1, 2, 3, 5, 6, 7, 9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF,
        0x10, 0x11, 0x12, 0x13, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B};
    for (const u16 kind : order)
    {
        if (word_at(records, 0) == 0)
        {
            continue;
        }
        for (u8* row = records + 8; row != nullptr; row = following_row(records, row))
        {
            const u16 row_kind = half_at(row, 4);
            if (row_kind == kind && row_kind < handlers.handlers.size())
            {
                if (handlers.untranslated[row_kind])
                {
                    char message[96];
                    std::snprintf(message, sizeof(message), "scene marker kind 0x%X handler is not translated", row_kind);
                    throw Error(message);
                }
                if (handlers.handlers[row_kind])
                {
                    handlers.handlers[row_kind](row);
                }
            }
        }
    }
}

// Xbox 0x00140020
void start_runtime_scene_markers(const u8* records, const RuntimeSceneMarkerRandomizer& randomizer,
    const RuntimeSceneMarkerHandlers& handlers)
{
    const u32 size = word_at(records, 4);
    auto* copy = static_cast<u8*>(engine_heap_allocate(i32(size)));
#if defined(GBTG_INSPECTOR_APP)
    const std::unique_ptr<u8, decltype(&engine_heap_free)> owner(copy, engine_heap_free);
#endif
    if (copy != nullptr)
    {
        std::memcpy(copy, records, size);
    }
    swap_runtime_marker_pairs(copy, randomizer);
    shuffle_runtime_marker_groups(copy, randomizer);
    dispatch_runtime_marker_records(copy, handlers);
#if !defined(GBTG_INSPECTOR_APP)
    engine_heap_free(copy);
#endif
}

// Xbox 0x0011F1C0
u16 next_runtime_marker_id(u16 id, i16 kind, i32 wrap, const u8* records)
{
    auto* block = const_cast<u8*>(records);
    const u32 count = word_at(block, 0);
    u8* first = block + 8;
    u8* start = find_marker_row(block, i16(id));
    if (start == nullptr)
    {
        if (count == 0)
        {
            return 0;
        }
        start = first;
    }
    if (kind == 0)
    {
        kind = i16(half_at(start, 4));
    }
    u8* row = start;
    while (true)
    {
        row = following_row(block, row);
        if (row == nullptr)
        {
            if (wrap == 0)
            {
                return 0;
            }
            row = count != 0 ? first : nullptr;
        }
        if (i16(half_at(row, 4)) == kind)
        {
            return half_at(row, 6);
        }
        if (row == start)
        {
            return 0;
        }
    }
}

// Xbox 0x00100DF0, XMM0 angle
float add_wrapped_angle(float angle, float delta)
{
    constexpr float turn = std::bit_cast<float>(0x40C90FDBu); // DAT_0032B298.
    const long double remainder = animation_remainder(angle + delta, turn); // 0x00152BE0
    float result = float(remainder);
    if (std::bit_cast<float>(0xC0490FDBu) > remainder) // DAT_0032B29C, compared in x87.
    {
        result = result + turn;
    }
    if (result >= std::bit_cast<float>(0x40490FDBu)) // DAT_0032B148.
    {
        result = result - turn;
    }
    return result;
}

// Xbox 0x00127FF0
void restart_runtime_marker_player(const u8* records, const RuntimeSceneMarkerHandlers& handlers)
{
    auto* block = const_cast<u8*>(records);
    const u32 count = word_at(block, 0);
    u8* row = block + 8;
    for (u32 index = 0; index < count; ++index)
    {
        if (half_at(row, 6) == next_runtime_marker_id(0, 2, 1, records))
        {
            const u16 kind = half_at(row, 4);
            if (kind < handlers.handlers.size() && handlers.untranslated[kind])
            {
                throw Error("scene marker restart names a kind whose handler is not translated");
            }
            if (kind < handlers.handlers.size() && handlers.handlers[kind])
            {
                handlers.handlers[kind](row);
            }
            return;
        }
        row += word_at(row, 0);
    }
}

u8* find_runtime_marker(const u8* records, i16 id)
{
    return find_marker_row(const_cast<u8*>(records), id);
}

// Xbox 0x0011F0B0
u16 farthest_runtime_marker(i16 kind, const Vec3& point, const u8* records)
{
    u16 chosen = 0;
    float farthest = -1000000000.f; // DAT_0032B308.
    const i32 count = count_runtime_markers(kind, records); // 0x0011EF50, ESI kind
    if (count < 1)
    {
        return 0;
    }
    for (i32 index = 0; index < count; ++index)
    {
        u16 id = next_runtime_marker_id(0, kind, 1, records); // 0x0011F1C0, AX 0
        for (i32 step = index; step > 0; --step)
        {
            id = next_runtime_marker_id(id, kind, 1, records);
        }
        const u8* row = find_runtime_marker(records, i16(id));
        if (row == nullptr)
        {
            throw Error("0x0011F0B0 reads the position of a marker with no row");
        }
        Vec3 at;
        std::memcpy(&at, row + 0xC, sizeof(at));
        const float distance = (point.z - at.z) * (point.z - at.z) + (point.y - at.y) * (point.y - at.y)
            + (point.x - at.x) * (point.x - at.x);
        if (farthest < distance)
        {
            farthest = distance;
            chosen = id;
        }
    }
    return chosen;
}

// Xbox 0x0011EF50
i32 count_runtime_markers(i16 kind, const u8* records)
{
    u16 id = next_runtime_marker_id(0, kind, 1, records);
    if (id == 0)
    {
        return 0;
    }
    i32 count = 1;
    id = next_runtime_marker_id(id, kind, 0, records);
    while (id != 0)
    {
        ++count;
        id = next_runtime_marker_id(id, kind, 0, records);
    }
    return count;
}

// Xbox 0x0011EFA0
u16 nearest_runtime_marker(const Vec3& position, i16 kind, const u8* records, Vec3 retained_position)
{
    u16 result = 0;
    float closest = std::bit_cast<float>(0x4E6E6B28u); // DAT_0032B30C.
    const i32 count = count_runtime_markers(kind, records);
    for (i32 index = 0; index < count; ++index)
    {
        u16 id = next_runtime_marker_id(0, kind, 1, records);
        for (i32 previous = index; previous > 0; --previous)
        {
            id = next_runtime_marker_id(id, kind, 1, records);
        }
        const u8* row = find_runtime_marker(records, i16(id));
        if (row != nullptr)
        {
            std::memcpy(&retained_position, row + 0xC, 12);
        }
        const float y = position.y - retained_position.y;
        const float z = position.z - retained_position.z;
        const float x = position.x - retained_position.x;
        const float distance = (z * z + y * y) + x * x;
        if (distance < closest)
        {
            closest = distance;
            result = id;
        }
    }
    return result;
}
}
