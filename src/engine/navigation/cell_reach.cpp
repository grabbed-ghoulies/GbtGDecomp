#include "cell_reach.h"
#include "cell_registration.h"
#include <cstring>

namespace gbtg
{
// Xbox 0x0010FE10
std::vector<RuntimeNavigationCellEntry> collect_navigation_cells_near(i32 floor, const Vec3& point, float inner,
    float radius)
{
    std::vector<RuntimeNavigationCellEntry> cells;
    const entity_detail::NavGrid* grid = entity_detail::g_nav_grids[floor];
    if (grid == nullptr)
    {
        return cells;
    }
    const Vec3 corners[4]{
        Vec3{point.x - radius, point.y, point.z - radius},
        Vec3{point.x + radius, point.y, point.z - radius},
        Vec3{point.x + radius, point.y, point.z + radius},
        Vec3{point.x - radius, point.y, point.z + radius},
    };
    i32 min_x = 0;
    i32 max_x = 0;
    i32 min_z = 0;
    i32 max_z = 0;
    entity_detail::navigation_cell_bounds(floor, corners, min_x, max_x, min_z, max_z);
    const float inner_squared = inner * inner;
    float reach = 0.f;
    if (entity_detail::g_nav_grids[0] != nullptr)
    {
        reach = float(entity_detail::g_nav_grids[0]->cell) * 0.5f; // DAT_0032B0F4.
    }
    reach = reach + radius;
    const float reach_squared = reach * reach;
    for (i32 x = min_x; x <= max_x; ++x)
    {
        for (i32 z = min_z; z <= max_z; ++z)
        {
            const i32 index = grid->width * z + x;
            if (radius > 0.f || inner > 0.f)
            {
                const entity_detail::NavCell& cell = grid->cells.at(size_t(index));
                const float dx = float(cell.x) - point.x;
                const float dz = float(cell.z) - point.z;
                const float distance = dz * dz + dx * dx;
                if (radius > 0.f && distance > reach_squared)
                {
                    continue;
                }
                if (inner > 0.f && inner_squared > distance)
                {
                    continue;
                }
            }
            cells.push_back(RuntimeNavigationCellEntry{u32(index), 0});
        }
    }
    return cells;
}

// Xbox 0x0012CEF0
u32 navigation_record_reaches_point(std::span<const u8, 0x20> record, const Vec3& eye, i32 region_mode,
    i32 allow_tagged, i32 reject_marked)
{
    const i32 floor = entity_detail::navigation_floor_index(eye, -1, 0);
    const Vec3 position{read_f32(record, 4), read_f32(record, 8), read_f32(record, 0xC)};
    const std::vector<RuntimeNavigationCellEntry> cells = collect_navigation_cells_near(floor, position, 0.f,
        read_f32(record, 0x1C));
    if (cells.empty())
    {
        return 1;
    }
    for (const RuntimeNavigationCellEntry& entry : cells)
    {
        if (entry.cell == 0xFFFFFFFFu)
        {
            continue;
        }
        const i32 cell_floor = entity_detail::navigation_floor_index(eye, -1, 0);
        const entity_detail::NavCell& cell = entity_detail::g_nav_grids[cell_floor]->cells.at(entry.cell);
        const Vec3 center{float(cell.x), float(cell.bottom), float(cell.z)};
        i32 last_cell = 0;
        if (entity_detail::navigation_path_clear(center, eye, region_mode, allow_tagged, 0, reject_marked, nullptr,
                &last_cell, 0))
        {
            return 1;
        }
    }
    return 0;
}
}
