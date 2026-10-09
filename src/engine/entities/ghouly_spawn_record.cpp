#include "ghouly_spawn_record.h"
#include "gbtg/entity.h"

#include <bit>

namespace gbtg
{
// Xbox 0x000F3920
GhoulySpawnRecord* allocate_ghouly_spawn_record()
{
    // Retail allocates 0xB4 and 0x520 bytes. Native pointer fields expand
    // the record and the 0x10-byte header; the two 0x288-byte rows stay raw.
    auto* record = static_cast<GhoulySpawnRecord*>(engine_heap_allocate(sizeof(GhoulySpawnRecord)));
    auto* pool = static_cast<GhoulySpawnPool*>(engine_heap_allocate(sizeof(GhoulySpawnPool) + 2 * 0x288));
    pool->stride = 0x288;
    pool->grow = 2;
    pool->begin = reinterpret_cast<u8*>(pool + 1);
    pool->cursor = pool->begin;
    pool->limit = pool->begin + 2 * 0x288;
    record->pool_8 = pool;
    record->value_18 = 0;
    record->value_1c = 0;
    record->prev = nullptr;
    record->next = nullptr;
    record->remaining_c = 0;
    record->value_10 = 0;
    record->value_b0 = 0;
    record->value_14 = 0;
    record->attributes_30[0] = '\0';
    record->index_28 = -1;
    record->timer_20 = std::bit_cast<float>(0x3A83126Fu); // DAT_0032B1A4
    return record;
}

// Xbox 0x000F3C30
void link_actor_load_record(ActorLoadRecord* head, ActorLoadRecord& record)
{
    if (record.prev != nullptr)
    {
        record.prev->next = record.next;
        if (record.next != nullptr)
        {
            record.next->prev = record.prev;
        }
        record.prev = nullptr;
        record.next = nullptr;
    }
    if (head != nullptr)
    {
        while (head->next != nullptr)
        {
            head = head->next;
        }
        head->next = &record;
        record.prev = head;
        record.next = nullptr;
    }
}

// Xbox 0x000F3A00
void release_ghouly_spawn_record(GhoulySpawnRecord& record)
{
    if (record.prev != nullptr)
    {
        record.prev->next = record.next;
        if (record.next != nullptr)
        {
            record.next->prev = record.prev;
        }
        record.prev = nullptr;
        record.next = nullptr;
    }
    // The retail frees both allocations through their owning heap slots.
    engine_heap_free(record.pool_8);
    engine_heap_free(&record);
}
}
