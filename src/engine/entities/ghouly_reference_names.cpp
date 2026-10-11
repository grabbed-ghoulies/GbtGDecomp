#include "ghouly_reference_names.h"
#include "entity_heap.h"
#include <cstring>

namespace gbtg
{
// Xbox 0x000F3C80
RuntimeReferenceNames** collect_runtime_ghouly_names(const GhoulySpawnRecord& record)
{
    const auto* definitions = record.pool_8;
    const i32 count = i32(definitions->cursor - definitions->begin) / definitions->stride;
    auto** owner = static_cast<RuntimeReferenceNames**>(engine_heap_allocate(i32(sizeof(RuntimeReferenceNames*))));
    *owner = allocate_runtime_reference_names(u32(count));
    const u8* cursor = record.pool_8->begin;
    const u8* end = record.pool_8->cursor;
    while (cursor < end)
    {
        if (read_u32({cursor, 0x288}, 0) == 0x5F646961)
        {
            retain_runtime_reference_name(*owner, reinterpret_cast<const char*>(cursor));
        }
        cursor += 0x288;
    }
    return owner;
}

// Xbox 0x0013E820
u16 runtime_executable_actor_type(const char* name,
    std::span<ExecutableResourcePool> resources)
{
    if (name != nullptr)
    {
        u32 prefix;
        std::memcpy(&prefix, name, 4);
        if (prefix == 0x5F646961)
        {
            auto* record = find_executable_resource(resources, name, true);
            if (record != nullptr && record->cpu.data() != nullptr)
            {
                return read_u16(record->cpu, 2);
            }
        }
    }
    return 0;
}
}
