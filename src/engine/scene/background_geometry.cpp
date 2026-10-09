#include "background_geometry.h"
#include "../entities/ghouly_box_decoder.h"

#include <cmath>
#include <cstring>

namespace gbtg
{
// Xbox 0x00043DD0
u32 collect_background_shape_event(const u8* event, GhoulySpawnPool*& pool)
{
    if (event[0x10] != 0 && (event[0xC] & 0xC) != 0)
    {
        if (pool == nullptr)
        {
            pool = static_cast<GhoulySpawnPool*>(engine_heap_allocate(
                sizeof(GhoulySpawnPool) + 10 * sizeof(BackgroundShapeEvent)));
            pool->begin = reinterpret_cast<u8*>(pool + 1);
            pool->cursor = pool->begin;
            pool->stride = sizeof(BackgroundShapeEvent);
            pool->grow = 10;
            pool->limit = pool->begin + 10 * sizeof(BackgroundShapeEvent);
        }
        if (pool->cursor == pool->limit)
        {
            pool = grow_ghouly_spawn_pool(pool);
        }
        auto* record = reinterpret_cast<BackgroundShapeEvent*>(pool->cursor);
        pool->cursor += pool->stride;
        record->source = event;
        record->flags = read_u32({event, 0x14}, 0xC);
    }
    return 1;
}

// Xbox 0x0003DBF0
void actor_model_bounds_low(const Actor& actor, Vec3& output)
{
    if (actor.model != nullptr && actor.model->cpu.data() != nullptr)
    {
        const auto cpu = actor.model->cpu;
        const u32 count = read_u32(cpu, 4);
        u32 index = 0;
        if (count != 0)
        {
            const u32 table = read_u32(cpu, 0);
            do
            {
                if (read_u32(cpu, table + index * 8) == 5)
                {
                    const u32 block = read_u32(cpu, table + index * 8 + 4);
                    if (block != 0)
                    {
                        output = Vec3{read_f32(cpu, block + 0x28),
                            read_f32(cpu, block + 0x2C), read_f32(cpu, block + 0x30)};
                        output.x = output.x * (actor.scale_x * actor.scale_200);
                        output.y = output.y * (actor.radius_y * actor.scale_200);
                        output.z = output.z * (actor.radius_xz * actor.scale_200);
                        return;
                    }
                    break;
                }
                ++index;
            }
            while (index < count);
        }
    }
    output.y = 0.f;
    output.x = 0.f;
    output.z = 0.f;
}

// Xbox 0x0003DCA0
void actor_model_bounds_high(const Actor& actor, Vec3& output)
{
    if (actor.model != nullptr && actor.model->cpu.data() != nullptr)
    {
        const auto cpu = actor.model->cpu;
        const u32 count = read_u32(cpu, 4);
        u32 index = 0;
        if (count != 0)
        {
            const u32 table = read_u32(cpu, 0);
            do
            {
                if (read_u32(cpu, table + index * 8) == 5)
                {
                    const u32 block = read_u32(cpu, table + index * 8 + 4);
                    if (block != 0)
                    {
                        output = Vec3{read_f32(cpu, block + 0x34),
                            read_f32(cpu, block + 0x38), read_f32(cpu, block + 0x3C)};
                        output.x = output.x * (actor.scale_x * actor.scale_200);
                        output.y = output.y * (actor.radius_y * actor.scale_200);
                        output.z = output.z * (actor.radius_xz * actor.scale_200);
                        return;
                    }
                    break;
                }
                ++index;
            }
            while (index < count);
        }
    }
    output.y = 0.f;
    output.x = 0.f;
    output.z = 0.f;
}

// Xbox 0x001198A0
void append_background_region(std::span<BackgroundRegion> regions, i32& count,
    const u8* source)
{
    auto& region = regions[count];
    region.source = source;
    std::memcpy(&region.first, source + 0x18, sizeof(Vec3));
    std::memcpy(&region.second, source + 0x24, sizeof(Vec3));
    const long double x = static_cast<long double>(region.first.x) - region.second.x;
    const long double y = static_cast<long double>(region.first.y) - region.second.y;
    const long double z = static_cast<long double>(region.first.z) - region.second.z;
    region.distance = float(std::sqrt((z * z + x * x) + y * y));
    ++count;
}
}
