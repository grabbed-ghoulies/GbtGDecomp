#include "mind_navigation_costs.h"
#include "../entities/mind_fields.h"
#include "../entities/player_mind_targets.h"
#include "cell_registration.h"
#include "../entities/entity_heap.h"
#include "../entities/ghouly_box_decoder.h"
#include "../audio/listener_settings.h"
#include "../entities/body_fields.h"
#include "../entities/player_fields.h"
#include <cstring>

namespace gbtg
{
// Xbox 0x00110110
GhoulySpawnPool* collect_runtime_navigation_costs(i32 floor, const Vec3& center, float radius)
{
    using namespace entity_detail;
    const auto* grid = g_nav_grids[floor];
    if (grid == nullptr)
    {
        return nullptr;
    }
    const Vec3 corners[4]{{center.x - radius, center.y, center.z - radius},
        {center.x + radius, center.y, center.z - radius},
        {center.x + radius, center.y, center.z + radius},
        {center.x - radius, center.y, center.z + radius}};
    i32 min_x, max_x, min_z, max_z;
    navigation_cell_bounds(floor, corners, min_x, max_x, min_z, max_z);
    const float half_cell = g_nav_grids[0] == nullptr ? 0.f : float(g_nav_grids[0]->cell) * .5f;
    const float outer = half_cell + radius;
    const float limit = outer * outer;
    GhoulySpawnPool* pool = nullptr;
    for (i32 x = min_x; x <= max_x; ++x)
    {
        for (i32 z = min_z; z <= max_z; ++z)
        {
            const i32 index = grid->width * z + x;
            const auto& cell = grid->cells[size_t(index)];
            const float dx = float(cell.x) - center.x;
            const float dz = float(cell.z) - center.z;
            if (radius > 0.f && dz * dz + dx * dx > limit)
            {
                continue;
            }
            if (pool == nullptr)
            {
                pool = static_cast<GhoulySpawnPool*>(engine_heap_allocate(i32(sizeof(GhoulySpawnPool) + 0x30)));
                pool->begin = reinterpret_cast<u8*>(pool + 1);
                pool->cursor = pool->begin;
                pool->stride = 0xC;
                pool->grow = 4;
                pool->limit = pool->begin + 0x30;
            }
            const float cost = float(u16(cell.value_8)) * .0625f;
            if (pool->cursor == pool->limit)
            {
                pool = grow_ghouly_spawn_pool(pool);
            }
            u8* row = pool->cursor;
            pool->cursor += pool->stride;
            const u32 zero = 0;
            std::memcpy(row, &index, 4);
            std::memcpy(row + 4, &zero, 4);
            std::memcpy(row + 8, &cost, 4);
        }
    }
    return pool;
}

// Xbox 0x0007B490
GhoulySpawnPool* suspend_runtime_mind_target_costs(std::span<const u8> actor,
    std::span<const u8> body, const Actor& body_actor, std::span<const u8> target)
{
    // Body +8's +0x38 is its native actor's radius_xz.
    const float radius = float((static_cast<long double>(body_actor.radius_xz)
        * body_record(body).radius_scale + mind_target_record(target).shape.radius) * 3.L);
    const Vec3 center{mind_target_record(target).shape.position.x, mind_target_record(target).shape.position.y,
        mind_target_record(target).shape.position.z};
    auto* costs = collect_runtime_navigation_costs(target[0x19], center, radius);
    if (costs != nullptr)
    {
        auto* grid = entity_detail::g_nav_grids[i32(player_record(actor).floor)];
        for (const u8* row = costs->begin; row < costs->cursor; row += 0xC)
        {
            const u32 index = read_u32({row, 0xC}, 0);
            auto& value = grid->cells[index].value_8;
            value = (value & 0xFFFF0000u) | 0xA0;
        }
    }
    return costs;
}

// Xbox 0x0007B510
void restore_runtime_mind_target_costs(std::span<const u8> actor, GhoulySpawnPool* costs)
{
    auto* grid = entity_detail::g_nav_grids[i32(player_record(actor).floor)];
    for (const u8* row = costs->begin; row < costs->cursor; row += 0xC)
    {
        const u16 original = u16(truncate_runtime_float(static_cast<long double>(read_f32({row, 0xC}, 8)) * 16.L));
        const u32 index = read_u32({row, 0xC}, 0);
        auto& value = grid->cells[index].value_8;
        value = (value & 0xFFFF0000u) | original;
    }
    engine_heap_free(costs);
}
}
