#include "segment_records.h"
#include "entity.h"
#include "entity_heap.h"
#include "gbtg/anim.h"
#include <bit>
#include <cstring>

namespace gbtg
{
// Xbox 0x000FCC90
GhoulySpawnPool* copy_segment_records(const entity_detail::SegmentPool& pool)
{
    const i32 used = i32(pool.cursor - pool.begin) / i32(pool.stride);
    const u32 count = u32(used) & 0xFFFF;
    auto* copy = static_cast<GhoulySpawnPool*>(engine_heap_allocate(
        i32(sizeof(GhoulySpawnPool) + count * 0x54)));
    copy->stride = 0x54;
    u32 grow = count;
    if (u16(used) == 0)
    {
        grow = 10;
    }
    u8* rows = reinterpret_cast<u8*>(copy + 1);
    copy->begin = rows;
    copy->cursor = rows;
    copy->grow = u16(grow);
    copy->limit = rows + count * 0x54;
    const u8* end = pool.cursor;
    for (const u8* source = pool.begin; source < end; source += 0x54)
    {
        if (copy->cursor == copy->limit)
        {
            const i32 kept = i32(copy->cursor - copy->begin) / i32(copy->stride);
            const i32 capacity = i32(copy->grow) + kept;
            copy = static_cast<GhoulySpawnPool*>(engine_heap_reallocate(copy,
                i32(sizeof(GhoulySpawnPool)) + i32(copy->stride) * capacity));
            u8* grown = reinterpret_cast<u8*>(copy + 1);
            copy->cursor = grown + size_t(copy->stride) * kept;
            copy->limit = grown + size_t(copy->stride) * capacity;
            copy->begin = grown;
        }
        u8* target = copy->cursor;
        copy->cursor = target + copy->stride;
        entity_copy_match_block(target, source);
    }
    return copy;
}

// Xbox 0x000FC6C0
void nearest_segment_contact(const GhoulySpawnPool& records, Vec3& output, const Vec3& reference)
{
    float best = std::bit_cast<float>(0x7F7FFFFFu); // DAT_0032B310.
    const u8* row = records.begin;
    const u8* end = records.cursor;
    output = reference;
    if (row < end)
    {
        u32 remaining = u32(end - row - 1) / 0x54 + 1;
        do
        {
            if (row[0x28] == 1)
            {
                Vec3 centre;
                float radius;
                std::memcpy(&centre, row + 0x2C, sizeof(centre));
                std::memcpy(&radius, row + 0x38, 4);
                float dx = reference.x - centre.x;
                float dz = reference.z - centre.z;
                float dy = reference.y - centre.y;
                const float length = dz * dz + dx * dx + dy * dy;
                if (length != 0.0f)
                {
                    const long double scale = retail_reciprocal_sqrt(length) * radius;
                    dx = float(dx * scale);
                    dy = float(dy * scale);
                    dz = float(dz * scale);
                }
                const float z = dz + centre.z;
                const float x = centre.x + dx;
                const float y = centre.y + dy;
                const float ez = z - reference.z;
                const float ey = y - reference.y;
                const float ex = x - reference.x;
                const float distance = ez * ez + ey * ey + ex * ex;
                if (best > distance)
                {
                    output.x = x;
                    output.y = y;
                    output.z = z;
                    best = distance;
                }
            }
            row += 0x54;
            --remaining;
        } while (remaining != 0);
    }
}
}
