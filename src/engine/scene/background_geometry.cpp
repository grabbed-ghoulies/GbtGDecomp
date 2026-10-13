#include "background_geometry.h"
#include "../entities/ghouly_box_decoder.h"
#include "../entities/actor_collision_culling.h"

#include <cmath>
#include <cstring>

namespace gbtg
{
u8 g_background_collision_bounds_enabled = 0;
Vec3 g_background_collision_bounds_low{};
Vec3 g_background_collision_bounds_high{};

// Xbox 0x00043C70
u32 accumulate_background_triangle_bounds(const u8* event, BackgroundCollisionBounds& bounds)
{
    for (u32 index = 0; index < 3; ++index)
    {
        const Vec3& point = bounds.vertices[read_u32({event, 0x14}, index * 4)];
        bounds.low.x = point.x > bounds.low.x ? bounds.low.x : point.x;
        bounds.low.y = point.y > bounds.low.y ? bounds.low.y : point.y;
        bounds.low.z = point.z > bounds.low.z ? bounds.low.z : point.z;
        bounds.high.x = bounds.high.x >= point.x ? bounds.high.x : point.x;
        bounds.high.y = bounds.high.y >= point.y ? bounds.high.y : point.y;
        bounds.high.z = bounds.high.z >= point.z ? bounds.high.z : point.z;
    }
    return 1;
}

// Xbox 0x00043190
void initialize_background_bounds(const Actor& actor, std::span<const Vec3> vertices,
    const std::unordered_map<const u8*, PhysicalShapeEventList>& lists)
{
    if (actor.model != nullptr && actor.model->cpu.data() != nullptr)
    {
        Vec3 low;
        Vec3 high;
        actor_model_bounds_low(actor, low);
        actor_model_bounds_high(actor, high);
        initialize_background_volume(low, high);
        if (actor.model != nullptr && actor.model->cpu.data() != nullptr &&
            actor.model->physics_shapes.data() != nullptr &&
            runtime_collision_shape_list_has_physical_shape(actor.model->physics_shapes))
        {
            BackgroundCollisionBounds bounds{{}, {}, vertices};
            PhysicalShapeEventContext context{0xFFFFFFFFu, 0, 0,
                [](const u8* event, void* user)
                {
                    return accumulate_background_triangle_bounds(event,
                        *static_cast<BackgroundCollisionBounds*>(user));
                }, &bounds, lists};
            visit_model_physical_shape_events(actor.model->physics_shapes, context);
            g_background_collision_bounds_low = bounds.low;
            g_background_collision_bounds_high = bounds.high;
            g_background_collision_bounds_enabled = 1;
        }
    }
}
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
