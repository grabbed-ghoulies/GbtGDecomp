#include "cell_registration.h"
#include <cstring>
#include <bit>

namespace gbtg::entity_detail
{
NavigationCellRegistration* g_nav_floor_connections[kNavSlots]{};

// Xbox 0x00110D00
u32 navigation_floor_connection_point(i32 source_floor, i32 target_floor,
    const Vec3& target, Vec3& output)
{
    i32 floor = target_floor;
    if (source_floor < target_floor)
    {
        floor = source_floor;
    }
    const NavigationCellRegistration* const connections = g_nav_floor_connections[floor];
    if (connections == nullptr)
    {
        return 0;
    }
    const u8* selected = nullptr;
    float best_distance = std::bit_cast<float>(u32(0x7F7FFFFF)); // DAT_0032B310.
    Vec3 selected_point;
    for (const u8* row = connections->begin; row < connections->end; row += 0xC)
    {
        u32 index;
        std::memcpy(&index, row, 4);
        const NavCell& cell = g_nav_grids[floor]->cells[index];
        const Vec3 candidate{float(cell.x), float(cell.bottom), float(cell.z)};
        if (navigation_cell_has_no_open_neighbor(*g_nav_grids[source_floor], index, 0))
        {
            continue;
        }
        const float dx = target.x - candidate.x;
        const float dy = target.y - candidate.y;
        const float dz = target.z - candidate.z;
        const float squared = (dz * dz + dx * dx) + dy * dy;
        if (selected != nullptr)
        {
            const float threshold = selected_point.y - 2.0f;
            if (source_floor < target_floor)
            {
                if (!(candidate.y >= threshold))
                {
                    continue;
                }
            }
            else if (!(threshold >= candidate.y))
            {
                continue;
            }
            if (!(best_distance > squared))
            {
                continue;
            }
        }
        selected = row;
        best_distance = squared;
        selected_point = candidate;
    }
    if (selected != nullptr)
    {
        output = selected_point;
        return 1;
    }
    return 0;
}
}
