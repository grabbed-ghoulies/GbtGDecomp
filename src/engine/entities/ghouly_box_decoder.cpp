#include "ghouly_box_decoder.h"
#include "gbtg/host_heap.h"
#include "gbtg/anim.h"
#include "gbtg/resource_error.h"

#include <bit>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <malloc.h>
#include <xmmintrin.h>

namespace gbtg
{
static_assert(sizeof(GhoulySpawnDefinition) == 0x288);
static_assert(offsetof(GhoulySpawnDefinition, weight) == 0x100);
static_assert(offsetof(GhoulySpawnDefinition, first_override) == 0x104);
static_assert(offsetof(GhoulySpawnDefinition, second_override) == 0x184);
static_assert(offsetof(GhoulySpawnDefinition, alternate) == 0x204);
static_assert(offsetof(GhoulySpawnDefinition, flags) == 0x284);

namespace
{
// Xbox 0x000F73A0
void* reallocate_spawn_pool(void* allocation, size_t bytes)
{
    const u8 heap_id = *(static_cast<const u8*>(allocation) + host_heap_block_size(allocation) - 2);
    void* grown = std::realloc(allocation, bytes + 4);
    if (grown != nullptr)
    {
        auto* footer = static_cast<u8*>(grown) + host_heap_block_size(grown) - 4;
        footer[0] = 0x34;
        footer[1] = 0x12;
        footer[2] = heap_id;
    }
    return grown;
}
}

// Xbox 0x000662A0
GhoulySpawnPool* grow_ghouly_spawn_pool(GhoulySpawnPool* pool)
{
    const size_t used = (pool->cursor - pool->begin) / pool->stride;
    const size_t count = used + pool->grow;
    const size_t bytes = sizeof(GhoulySpawnPool) + pool->stride * count;
    auto* grown = static_cast<GhoulySpawnPool*>(reallocate_spawn_pool(pool, bytes));
    grown->begin = reinterpret_cast<u8*>(grown + 1);
    grown->cursor = grown->begin + grown->stride * used;
    grown->limit = grown->begin + grown->stride * count;
    return grown;
}

namespace
{
GhoulySpawnDefinition& next_definition(GhoulySpawnRecord& record)
{
    GhoulySpawnPool* pool = record.pool_8;
    if (pool->cursor == pool->limit)
    {
        pool = grow_ghouly_spawn_pool(pool);
        record.pool_8 = pool;
    }
    auto* definition = reinterpret_cast<GhoulySpawnDefinition*>(pool->cursor);
    pool->cursor += pool->stride;
    return *definition;
}
}

// Xbox 0x000F3AB0
void append_ghouly_spawn_definition(GhoulySpawnRecord& record, const char* name,
    const char* variant, float weight, const char* first_override,
    const char* second_override, u32 flags)
{
    GhoulySpawnDefinition& definition = next_definition(record);
    std::strcpy(definition.name, name);
    std::strcpy(definition.variant, variant);
    definition.weight = weight;
    std::strcpy(definition.first_override, first_override);
    std::strcpy(definition.second_override, second_override);
    definition.alternate[0] = '\0';
    definition.flags = flags;
    record.value_b0 = 1;
}

// Xbox 0x000F3B60
void append_ghouly_spawn_variant(GhoulySpawnRecord& record, const char* variant)
{
    GhoulySpawnDefinition& definition = next_definition(record);
    definition.name[0] = '\0';
    definition.alternate[0] = '\0';
    std::strcpy(definition.variant, variant);
    definition.weight = 1.f;
    definition.first_override[0] = '\0';
    definition.flags = 0;
}

// Xbox 0x000F3BD0
void append_ghouly_spawn_alternate(GhoulySpawnRecord& record, const char* alternate)
{
    GhoulySpawnDefinition& definition = next_definition(record);
    definition.name[0] = '\0';
    definition.variant[0] = '\0';
    std::strcpy(definition.alternate, alternate);
    definition.weight = 0.f;
    definition.first_override[0] = '\0';
    definition.flags = 0;
}

// Xbox 0x000F39A0
GhoulySpawnRecord* load_ghoulybox(const char* name, AnimationResourceRegistry& registry,
    ResourceErrorState& error, u32& random_state)
{
    GhoulySpawnRecord* record = nullptr;
    if (std::memcmp(name, "aid_", 4) == 0)
    {
        const auto resource = acquire_animation_resource(registry, name);
        if (resource.data() == nullptr)
        {
            resource_error(error, 2);
            return nullptr;
        }
        record = allocate_ghouly_spawn_record();
        record->remaining_c = 1;
        decode_ghoulybox(*record, resource, random_state);
        release_animation_resource(registry, resource);
    }
    return record;
}

// Xbox 0x000517B0
void decode_ghoulybox(GhoulySpawnRecord& record, std::span<const u8> resource,
    u32& random_state)
{
    size_t cursor = 0;
    u32 type = read_u32(resource, cursor + 4);
    while (type != 0)
    {
        const u8* node = resource.data() + cursor;
        switch (type)
        {
        case 1:
            append_ghouly_spawn_definition(record, reinterpret_cast<const char*>(node + 8),
                reinterpret_cast<const char*>(node + 0x8C),
                float(std::bit_cast<i32>(read_u32(resource, cursor + 0x88))) * std::bit_cast<float>(0x3C23D70Au),
                reinterpret_cast<const char*>(node + 0x10C), reinterpret_cast<const char*>(node + 0x18C),
                read_u32(resource, cursor + 0x20C));
            break;
        case 2:
            append_ghouly_spawn_variant(record, reinterpret_cast<const char*>(node + 8));
            break;
        case 3:
            record.remaining_c = std::bit_cast<i32>(read_u32(resource, cursor + 8));
            break;
        case 4:
        {
            random_state = random_state * 0x19660Du + 0x3C6EF35Fu;
            const float random = std::bit_cast<float>((random_state & 0x7FFFFFu) | 0x3F800000u) - 1.f;
            const u32 minimum = read_u32(resource, cursor + 8);
            const u32 maximum = read_u32(resource, cursor + 0xC);
            const i32 width = std::bit_cast<i32>(maximum - minimum + 1);
            record.remaining_c = _mm_cvttss_si32(_mm_set_ss(float(width) * random + float(std::bit_cast<i32>(minimum))));
            break;
        }
        case 5:
            record.remaining_c = -1;
            break;
        case 6:
            record.value_14 = read_u32(resource, cursor + 8);
            break;
        case 7:
            record.value_18 = read_u32(resource, cursor + 8);
            record.value_1c = read_u32(resource, cursor + 0xC);
            record.timer_20 = std::bit_cast<float>(0x3A83126Fu);
            break;
        case 8:
            append_ghouly_spawn_alternate(record, reinterpret_cast<const char*>(node + 8));
        }
        cursor += read_u32(resource, cursor);
        type = read_u32(resource, cursor + 4);
    }
}
}
