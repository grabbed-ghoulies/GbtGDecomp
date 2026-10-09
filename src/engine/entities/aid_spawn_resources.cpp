#include "aid_spawn_resources.h"
#include "ghouly_box_decoder.h"

#include <bit>
#include <cstring>

namespace gbtg
{
AidSpawnQueueRecord g_aid_spawn_queue{};

namespace
{
GhoulySpawnPool* allocate_pool(u16 stride, u16 grow)
{
    auto* pool = static_cast<GhoulySpawnPool*>(engine_heap_allocate(sizeof(GhoulySpawnPool) + stride * grow));
    pool->stride = stride;
    pool->grow = grow;
    pool->begin = reinterpret_cast<u8*>(pool + 1);
    pool->cursor = pool->begin;
    pool->limit = pool->begin + stride * grow;
    return pool;
}

void append_interval(SpawnPoolSelection& selection, float value)
{
    if (selection.pool->cursor == selection.pool->limit)
    {
        selection.pool = grow_ghouly_spawn_pool(selection.pool);
    }
    u8* row = selection.pool->cursor;
    selection.pool->cursor += selection.pool->stride;
    std::memcpy(row, &value, 4);
}
}

// Xbox 0x000F3640
SpawnPoolSelection* allocate_spawn_string_selection()
{
    auto* selection = static_cast<SpawnPoolSelection*>(engine_heap_allocate(sizeof(SpawnPoolSelection)));
    selection->pool = allocate_pool(0x80, 2);
    selection->sequence = 0;
    selection->mode = 0;
    selection->previous = -1;
    return selection;
}

// Xbox 0x000F3690
void append_spawn_string(SpawnPoolSelection& selection, const char* name)
{
    if (selection.pool->cursor == selection.pool->limit)
    {
        selection.pool = grow_ghouly_spawn_pool(selection.pool);
    }
    u8* row = selection.pool->cursor;
    selection.pool->cursor += selection.pool->stride;
    std::strcpy(reinterpret_cast<char*>(row), name);
}

// Xbox 0x000F37B0
void release_spawn_selection(SpawnPoolSelection* selection)
{
    engine_heap_free(selection->pool);
    engine_heap_free(selection);
}

// Xbox 0x000F5370
SpawnControllerSelection* allocate_spawn_controller_selection()
{
    auto* selection = static_cast<SpawnControllerSelection*>(engine_heap_allocate(sizeof(SpawnControllerSelection)));
    selection->pool = allocate_pool(8, 2);
    selection->sequence = 0;
    selection->previous = -1;
    selection->distinct = 0;
    selection->occurrences.fill(0);
    selection->mode = 0;
    return selection;
}

// Xbox 0x000F53D0
void append_spawn_controller(SpawnControllerSelection& selection, u32 key)
{
    if (selection.pool->cursor == selection.pool->limit)
    {
        selection.pool = grow_ghouly_spawn_pool(selection.pool);
    }
    u8* row = selection.pool->cursor;
    selection.pool->cursor += selection.pool->stride;
    std::memcpy(row, &key, 4);
    const u32 used = 0;
    std::memcpy(row + 4, &used, 4);
    const u8 previous = selection.occurrences[key];
    ++selection.occurrences[key];
    if (previous == 0)
    {
        ++selection.distinct;
    }
}

// Xbox 0x000F5110
void set_spawn_interval_range(SpawnPoolSelection& selection, float low, float high)
{
    selection.mode = 1;
    selection.pool->cursor = selection.pool->begin;
    append_interval(selection, low);
    append_interval(selection, high);
}

// Xbox 0x000F5170
void set_spawn_interval_ramp(SpawnPoolSelection& selection, float initial, float limit, float step)
{
    selection.mode = 3;
    selection.pool->cursor = selection.pool->begin;
    append_interval(selection, initial);
    append_interval(selection, limit);
    append_interval(selection, step);
}

// Xbox 0x00051940
void decode_aid_spawn_queue(AidSpawnQueueRecord& record, const u8* resource)
{
    auto* names = allocate_spawn_string_selection();
    auto* attributes = allocate_spawn_string_selection();
    auto* controllers = allocate_spawn_controller_selection();
    auto* intervals = static_cast<SpawnPoolSelection*>(engine_heap_allocate(sizeof(SpawnPoolSelection)));
    intervals->pool = allocate_pool(4, 2);
    intervals->sequence = 0;
    intervals->previous = i32(0x3A83126F);
    intervals->mode = 0;
    u32 kind;
    std::memcpy(&kind, resource + 4, 4);
    while (kind != 0)
    {
        const std::span<const u8> node{resource, 24};
        switch (kind)
        {
        case 1:
            names->mode = read_u32(node, 8);
            break;
        case 2:
            append_spawn_string(*names, reinterpret_cast<const char*>(resource + 8));
            break;
        case 3:
            controllers->mode = read_u32(node, 8);
            break;
        case 4:
            append_spawn_controller(*controllers, read_u32(node, 8));
            break;
        case 5:
            intervals->mode = 0;
            append_interval(*intervals, read_f32(node, 8));
            break;
        case 6:
            set_spawn_interval_range(*intervals, read_f32(node, 8), read_f32(node, 12));
            break;
        case 7:
        {
            float step;
            if (read_u32(node, 12) == 0)
            {
                step = 0.f - read_f32(node, 16);
            }
            else
            {
                step = read_f32(node, 16);
            }
            set_spawn_interval_ramp(*intervals, read_f32(node, 8), read_f32(node, 20), step);
            break;
        }
        case 8:
            intervals->mode = 0;
            break;
        case 9:
            append_interval(*intervals, read_f32(node, 8));
            break;
        case 10:
            record.remaining = -1;
            break;
        case 11:
            record.remaining = std::bit_cast<i32>(read_u32(node, 8));
            break;
        case 12:
            record.remaining = -2;
            break;
        case 13:
            attributes->mode = read_u32(node, 8);
            break;
        case 14:
            append_spawn_string(*attributes, reinterpret_cast<const char*>(resource + 8));
            break;
        case 15:
            record.remaining = -3;
            record.value_1c = i16(read_u16(node, 8));
            break;
        default:
            break;
        }
        resource += read_u32(node, 0);
        std::memcpy(&kind, resource + 4, 4);
    }
    if (record.names != nullptr)
    {
        release_spawn_selection(record.names);
    }
    record.names = names;
    if (record.attributes != nullptr)
    {
        release_spawn_selection(record.attributes);
    }
    record.attributes = attributes;
    if (record.controllers != nullptr)
    {
        release_spawn_selection(record.controllers);
    }
    record.controllers = controllers;
    if (record.intervals != nullptr)
    {
        release_spawn_selection(record.intervals);
    }
    record.intervals = intervals;
}

// Xbox 0x000F4E50
AidSpawnQueueRecord* load_aid_spawn_queue(const char* name, AnimationResourceRegistry& registry,
    ResourceErrorState& error)
{
    AidSpawnQueueRecord* record = nullptr;
    if (std::memcmp(name, "aid_", 4) == 0)
    {
        const auto resource = acquire_animation_resource(registry, name);
        if (resource.data() == nullptr)
        {
            resource_error(error, 2);
            return nullptr;
        }
        record = static_cast<AidSpawnQueueRecord*>(engine_heap_allocate(sizeof(AidSpawnQueueRecord)));
        record->previous = nullptr;
        record->next = nullptr;
        record->names = nullptr;
        record->attributes = nullptr;
        record->controllers = nullptr;
        record->intervals = nullptr;
        record->remaining = 0;
        record->value_1c = 0;
        record->current = nullptr;
        auto* head = &g_aid_spawn_queue;
        while (head->next != nullptr)
        {
            head = head->next;
        }
        head->next = record;
        record->previous = head;
        record->next = nullptr;
        decode_aid_spawn_queue(*record, resource.data());
        release_animation_resource(registry, resource);
    }
    return record;
}

// Xbox 0x000F4EE0
void release_aid_spawn_queue(AidSpawnQueueRecord* record)
{
    if (record->previous != nullptr)
    {
        record->previous->next = record->next;
        if (record->next != nullptr)
        {
            record->next->previous = record->previous;
        }
        record->previous = nullptr;
        record->next = nullptr;
    }
    if (record->names != nullptr)
    {
        release_spawn_selection(record->names);
    }
    record->names = nullptr;
    if (record->attributes != nullptr)
    {
        release_spawn_selection(record->attributes);
    }
    record->attributes = nullptr;
    if (record->controllers != nullptr)
    {
        release_spawn_selection(record->controllers);
    }
    record->controllers = nullptr;
    if (record->intervals != nullptr)
    {
        release_spawn_selection(record->intervals);
    }
    record->intervals = nullptr;
    engine_heap_free(record);
}
}
