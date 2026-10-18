#include "navigation.h"
#include "floor_tracking.h"
#include "gbtg/anim.h"

#include <bit>
#include <cmath>
#include <cstddef>
#include <xmmintrin.h>

namespace gbtg
{
namespace entity_detail
{
// Xbox 0x001117A0
u8 navigation_surface_kind(i32 floor, const Vec3& point)
{
    const auto* grid = g_nav_grids[floor];
    if (grid != nullptr)
    {
        const i32 index = navigation_cell_index(floor, point);
        if (index != -1)
        {
            const auto* cell = reinterpret_cast<const u8*>(&grid->cells[index]);
            if (cell[0xB] != 0)
            {
                return cell[0xB];
            }
            return cell[0xA];
        }
    }
    return 0;
}

static_assert(sizeof(NavCell) == 0x12);
static_assert(offsetof(NavCell, bottom) == 2);
static_assert(offsetof(NavCell, flags_7) == 7);
static_assert(offsetof(NavCell, region) == 0xC);
static_assert(offsetof(NavCell, top) == 0xE);
static_assert(offsetof(NavCell, connections) == 0x10);
static_assert(offsetof(NavCell, flags_11) == 0x11);

namespace
{
// DAT_002FD000 / DAT_002FD004, stride 8; this is the retail bit order.
constexpr i32 neighbor_x[8] = {-1, 1, 0, 0, -1, 1, -1, 1};
constexpr i32 neighbor_z[8] = {0, 0, 1, -1, 1, 1, -1, -1};

void discard_neighbor_distance(const NavCell& first, const NavCell& second)
{
    const float dx = float(i32(first.x) - i32(second.x));
    const float dy = float(i32(first.bottom) - i32(second.bottom));
    const float dz = float(i32(first.z) - i32(second.z));
    const float squared = dz * dz + (dy * dy + dx * dx);
    if (squared != 0.f)
    {
        // 0x00152B30 computes the square root; both callers discard its result.
        const volatile long double distance = retail_square_root(squared);
        (void)distance;
    }
}
}

// Xbox 0x00111660
i32 navigation_nearest_open_cell(u32 index, i32 floor, Vec3* point)
{
    const NavGrid& grid = *g_nav_grids[floor];
    const u32 x = index % u32(grid.width);
    const u32 z = index / u32(grid.width);
    i32 radius = 1;
    for (;;)
    {
        i32 outside = 0;
        for (i32 direction = 0; direction < 8; ++direction)
        {
            const i32 nx = std::bit_cast<i32>(x - u32(neighbor_x[direction]) * u32(radius));
            const i32 nz = std::bit_cast<i32>(z - u32(neighbor_z[direction]) * u32(radius));
            if (nx < 0 || nx >= grid.width || nz < 0 || nz >= grid.depth)
            {
                ++outside;
            }
            else
            {
                const NavGrid& current_grid = *g_nav_grids[floor];
                const i32 candidate = current_grid.width * nz + nx;
                const NavCell& cell = current_grid.cells[candidate];
                if ((cell.flags_7 & 2) == 0 && (cell.value_6 & 0x1F) == 0 &&
                    ((cell.region & 0xFFF) == 0 || (cell.region & 0x1000) == 0))
                {
                    if (point == nullptr)
                    {
                        return candidate;
                    }
                    const NavCell& selected = g_nav_grids[floor]->cells[candidate];
                    *point = Vec3{float(selected.x), float(selected.bottom), float(selected.z)};
                    return candidate;
                }
            }
        }
        if (outside == 8)
        {
            return -1;
        }
        radius = std::bit_cast<i32>(u32(radius) + 1);
    }
}

// Xbox 0x00036A60
float navigation_cell_distance_cost(const NavGrid& grid, u32 source, u32 destination)
{
    const NavCell& first = grid.cells[source];
    const NavCell& second = grid.cells[destination];
    const float dx = float(i32(first.x) - i32(second.x));
    const float dy = float(i32(first.bottom) - i32(second.bottom));
    const float dz = float(i32(first.z) - i32(second.z));
    const float squared = dz * dz + (dy * dy + dx * dx);
    const float distance = squared != 0.0f ? float(retail_square_root(squared)) : 0.0f;
    return grid.cost_scale * distance;
}

// Xbox 0x00037340
bool navigation_search_cells_connected(const NavigationPathSearch& search,
    u32 source, i32 destination)
{
    i32 direction = 0;
    for (;;)
    {
        const NavGrid& grid = *search.grid;
        const i32 x = static_cast<i32>(source % static_cast<u32>(grid.width));
        const i32 z = static_cast<i32>(source / static_cast<u32>(grid.width));
        if (direction >= 8)
        {
            return false;
        }
        const NavCell& cell = grid.cells[source];
        while (direction < 8)
        {
            const i32 next_x = x + neighbor_x[direction];
            const i32 next_z = z + neighbor_z[direction];
            const u32 bit = 1u << direction;
            ++direction;
            if ((cell.connections & bit) == 0 || next_x < 0 || next_x >= grid.width ||
                next_z < 0 || next_z >= grid.depth)
            {
                continue;
            }
            const i32 next = next_z * grid.width + next_x;
            discard_neighbor_distance(cell, grid.cells[next]);
            if (destination == next)
            {
                return true;
            }
            goto next_connected_neighbor;
        }
        return false;
next_connected_neighbor:
        ;
    }
}

// Xbox 0x0012CB80
bool navigation_cell_allowed(const NavGrid& grid, u32 index, i32 reject_marked,
    i32 region_mode, i32 allow_tagged, i32 query_occupancy, const OccupancyRecord* allowed,
    i16 group)
{
    const NavCell& cell = grid.cells[index];
    if (reject_marked != 0 && std::bit_cast<signed char>(cell.flags_11) < 0)
    {
        return false;
    }
    if (region_mode == 0)
    {
        if ((cell.flags_7 & 4) == 0)
        {
            if (allow_tagged == 0)
            {
                return false;
            }
            if ((cell.flags_11 & 0x3F) == 0)
            {
                return false;
            }
        }
    }
    else
    {
        if ((cell.flags_7 & 2) != 0)
        {
            return false;
        }
        const u16 region = cell.region;
        if ((region & 0xFFF) != 0 && (region & 0x1000) != 0)
        {
            return false;
        }
    }
    if (query_occupancy != 0)
    {
        const Vec3 point{float(cell.x), float(cell.bottom), float(cell.z)};
        const OccupancyRecord* found = find_occupancy_record(point, 0.f, 4, nullptr, group);
        if (found != nullptr && found != allowed)
        {
            return false;
        }
    }
    return true;
}

// Xbox 0x00110BF0
i32 navigation_floor_index(const Vec3& point, i32 preferred, i32 check_cell)
{
    i32 best = 0;
    i32 floor = 0;
    if (g_nav_count > 0)
    {
        do
        {
            if (floor == preferred)
            {
                if (check_cell != 0)
                {
                    const i32 cell = navigation_cell_index(floor, point);
                    if (cell == -1 || (g_nav_grids[floor]->cells[cell].flags_7 & 2) == 0)
                    {
                        return floor;
                    }
                }
                if (g_nav_min_y[floor] <= point.y && point.y <= g_nav_max_y[floor] + 2.f)
                {
                    return floor;
                }
            }
            if (point.y > g_nav_min_y[floor])
            {
                best = floor;
            }
            ++floor;
        }
        while (floor < g_nav_count);
    }
    return best;
}

// Xbox 0x00110BB0, ESI floor, EBX point
bool navigation_cell_height(i32 floor, const Vec3& point, float& height)
{
    const i32 cell = navigation_cell_index(floor, point); // 0x00110EF0
    if (cell == -1)
    {
        return false;
    }
    height = float(g_nav_grids[floor]->cells[size_t(cell)].bottom);
    return true;
}

// Xbox 0x00110EF0
i32 navigation_cell_index(i32 floor, const Vec3& point)
{
    const NavGrid* grid = g_nav_grids[floor];
    if (grid == nullptr)
    {
        return -1;
    }
    const i32 local_x = std::bit_cast<i32>(u32(_mm_cvttss_si32(_mm_set_ss(point.x))) - u32(g_nav_origin_x));
    const i32 local_z = std::bit_cast<i32>(u32(_mm_cvttss_si32(_mm_set_ss(point.z))) - u32(g_nav_origin_z));
    const i32 x = local_x / grid->cell;
    const i32 z = local_z / grid->cell;
    if (x >= 0 && x < grid->width && z >= 0 && z < grid->depth)
    {
        return std::bit_cast<i32>(u32(grid->width) * u32(z) + u32(x));
    }
    return -1;
}

// Xbox 0x0012CC50
bool navigation_path_clear(const Vec3& source, const Vec3& destination, i32 region_mode,
    i32 allow_tagged, i32 query_occupancy, i32 reject_marked, const OccupancyRecord* allowed,
    i32* last_cell, i16 group)
{
    if (g_nav_grids[0] == nullptr)
    {
        return false;
    }
    const i32 source_floor = navigation_floor_index(source, -1, 0);
    const i32 destination_floor = navigation_floor_index(destination, -1, 0);
    const NavGrid& grid = *g_nav_grids[source_floor];
    if (source_floor == destination_floor)
    {
        i32 x;
        i32 z;
        i32 end_x;
        i32 end_z;
        if (navigation_cell_coordinates(source_floor, source, x, z) &&
            navigation_cell_coordinates(source_floor, destination, end_x, end_z))
        {
            i32 cell = grid.width * z + x;
            if (x == end_x && z == end_z)
            {
                return true;
            }
            const i32 dx = end_x - x;
            const i32 dz = end_z - z;
            i32 twice_x = dx;
            if (dx < 0)
            {
                twice_x = -dx;
            }
            twice_x *= 2;
            i32 twice_z = dz;
            if (dz < 0)
            {
                twice_z = -dz;
            }
            twice_z *= 2;
            const i32 step_x = i32(dx >= 0) * 2 - 1;
            const i32 step_z = i32(dz >= 0) * 2 - 1;
            i32 previous;
            if (twice_z < twice_x)
            {
                i32 error = twice_z - twice_x / 2;
                do
                {
                    previous = cell;
                    if (error >= 0)
                    {
                        z += step_z;
                        error -= twice_x;
                    }
                    const i32 next_error = error + twice_z;
                    x += step_x;
                    cell = grid.width * z + x;
                    if (allow_tagged == 0 && !navigation_cells_connected(grid, u32(cell), previous))
                    {
                        return false;
                    }
                    if (x == end_x)
                    {
                        return true;
                    }
                    if (!navigation_cell_allowed(grid, u32(cell), reject_marked, region_mode,
                        allow_tagged, query_occupancy, allowed, group))
                    {
                        break;
                    }
                    error = next_error;
                }
                while (true);
            }
            else
            {
                i32 error = twice_x - twice_z / 2;
                do
                {
                    previous = cell;
                    if (error >= 0)
                    {
                        x += step_x;
                        error -= twice_z;
                    }
                    z += step_z;
                    const i32 next_error = error + twice_x;
                    cell = grid.width * z + x;
                    if (allow_tagged == 0 && !navigation_cells_connected(grid, u32(cell), previous))
                    {
                        return false;
                    }
                    if (z == end_z)
                    {
                        return true;
                    }
                    if (!navigation_cell_allowed(grid, u32(cell), reject_marked, region_mode,
                        allow_tagged, query_occupancy, allowed, group))
                    {
                        break;
                    }
                    error = next_error;
                }
                while (true);
            }
            *last_cell = previous;
        }
    }
    else
    {
        const i32 source_cell = navigation_cell_index(source_floor, source);
        const i32 destination_cell = navigation_cell_index(destination_floor, destination);
        if (source_cell != -1 && destination_cell != -1 &&
            (grid.cells[source_cell].flags_11 & 0x40) != 0 &&
            (g_nav_grids[destination_floor]->cells[destination_cell].flags_11 & 0x40) != 0)
        {
            return true;
        }
    }
    return false;
}

// Xbox 0x00110E80
bool navigation_cell_coordinates(i32 floor, const Vec3& point, i32& x, i32& z)
{
    const NavGrid* grid = g_nav_grids[floor];
    if (grid == nullptr)
    {
        return false;
    }
    const i32 local_x = std::bit_cast<i32>(u32(_mm_cvttss_si32(_mm_set_ss(point.x))) - u32(g_nav_origin_x));
    const i32 local_z = std::bit_cast<i32>(u32(_mm_cvttss_si32(_mm_set_ss(point.z))) - u32(g_nav_origin_z));
    const i32 cell_x = local_x / grid->cell;
    const i32 cell_z = local_z / grid->cell;
    if (cell_x >= 0 && cell_x < grid->width && cell_z >= 0 && cell_z < grid->depth)
    {
        x = cell_x;
        z = cell_z;
        return true;
    }
    return false;
}

// Xbox 0x0012CA40
bool navigation_cells_connected(const NavGrid& grid, u32 destination, i32 source)
{
    const NavCell& cell = grid.cells[destination];
    if (cell.connections == 0xFF)
    {
        return true;
    }
    i32 direction = 0;
    do
    {
        if (direction > 7)
        {
            return false;
        }
        i32 x;
        i32 z;
        while (true)
        {
            x = std::bit_cast<i32>(destination % u32(grid.width) + u32(neighbor_x[direction]));
            z = std::bit_cast<i32>(destination / u32(grid.width) + u32(neighbor_z[direction]));
            const i32 bit = direction;
            ++direction;
            if ((cell.connections & (1u << bit)) != 0 && x >= 0 && x < grid.width &&
                z >= 0 && z < grid.depth)
            {
                break;
            }
            if (direction > 7)
            {
                return false;
            }
        }
        const i32 neighbor = grid.width * z + x;
        discard_neighbor_distance(cell, grid.cells[neighbor]);
        if (neighbor == source)
        {
            return true;
        }
    }
    while (true);
}

// Xbox 0x00036560
u32 navigation_neighbor_direction_mask(i32 x, u32 z)
{
    i32 index = 0;
    while (neighbor_x[index] != x || u32(neighbor_z[index]) != z)
    {
        ++index;
        if (index > 7)
        {
            return z & 0xFFFFFF00u;
        }
    }
    return 1u << (u32(index) & 0x1F);
}

// Xbox 0x00036590
bool navigation_neighbor_connected(const NavGrid& grid, u32 source, u32 destination)
{
    if (source == destination)
    {
        return true;
    }
    const u32 width = u32(grid.width);
    const i32 x = std::bit_cast<i32>(destination % width - source % width);
    const i32 z = std::bit_cast<i32>(destination / width - source / width);
    i32 absolute_x = x;
    if (absolute_x < 0)
    {
        absolute_x = std::bit_cast<i32>(0u - u32(absolute_x));
    }
    if (absolute_x < 2)
    {
        i32 absolute_z = z;
        if (absolute_z < 0)
        {
            absolute_z = std::bit_cast<i32>(0u - u32(absolute_z));
        }
        if (absolute_z < 2
            && (grid.cells[source].connections & u8(navigation_neighbor_direction_mask(x, u32(z)))) != 0)
        {
            return true;
        }
    }
    return false;
}

// Xbox 0x00036600
bool navigation_cell_has_no_open_neighbor(const NavGrid& grid, u32 index, u32 use_region)
{
    const NavCell& cell = grid.cells[index];
    u16 region;
    if (use_region == 0)
    {
        region = 0;
    }
    else
    {
        region = cell.region & 0xFFF;
    }
    i32 direction = 0;
    do
    {
        const i32 x = std::bit_cast<i32>(index % u32(grid.width) + u32(neighbor_x[direction]));
        const i32 z = std::bit_cast<i32>(index / u32(grid.width) + u32(neighbor_z[direction]));
        const i32 bit = direction;
        ++direction;
        if ((cell.connections & (1u << bit)) != 0 && x >= 0 && x < grid.width
            && z >= 0 && z < grid.depth)
        {
            const NavCell& neighbor = grid.cells[grid.width * z + x];
            if ((region != 0 && (neighbor.region & 0xFFF) == region)
                || ((neighbor.flags_7 & 2) == 0
                    && ((neighbor.region & 0xFFF) == 0 || (neighbor.region & 0x1000) == 0)))
            {
                if ((neighbor.value_6 & 0x1F) == 0 || (neighbor.region & 0x8000) != 0
                    || (neighbor.region & 0xFFF) != 0)
                {
                    discard_neighbor_distance(cell, neighbor);
                    return false;
                }
            }
        }
        if (direction > 7)
        {
            return true;
        }
    }
    while (true);
}

// Xbox 0x000368C0
bool navigation_cell_has_no_unblocked_neighbor(const NavGrid& grid, u32 index)
{
    const NavCell& cell = grid.cells[index];
    i32 direction = 0;
    do
    {
        const i32 x = std::bit_cast<i32>(index % u32(grid.width) + u32(neighbor_x[direction]));
        const i32 z = std::bit_cast<i32>(index / u32(grid.width) + u32(neighbor_z[direction]));
        const i32 bit = direction;
        ++direction;
        if ((cell.connections & (1u << bit)) != 0 && x >= 0 && x < grid.width
            && z >= 0 && z < grid.depth)
        {
            const NavCell& neighbor = grid.cells[grid.width * z + x];
            discard_neighbor_distance(cell, neighbor);
            if ((neighbor.flags_7 & 2) == 0
                && ((neighbor.region & 0xFFF) == 0 || (neighbor.region & 0x1000) == 0))
            {
                return false;
            }
        }
        if (direction > 7)
        {
            return true;
        }
    }
    while (true);
}

// Xbox 0x00036770
bool navigation_cell_is_isolated(const NavGrid& grid, u32 index, bool use_region)
{
    const NavCell& cell = grid.cells[index];
    u16 region;
    if (!use_region)
    {
        region = 0;
    }
    else
    {
        region = cell.region & 0xFFF;
    }
    i32 direction = 0;
    do
    {
        const i32 x = std::bit_cast<i32>(index % u32(grid.width) + u32(neighbor_x[direction]));
        const i32 z = std::bit_cast<i32>(index / u32(grid.width) + u32(neighbor_z[direction]));
        const i32 bit = direction;
        ++direction;
        if ((cell.connections & (1u << bit)) != 0 && x >= 0 && x < grid.width &&
            z >= 0 && z < grid.depth)
        {
            const NavCell& neighbor = grid.cells[grid.width * z + x];
            if ((region != 0 && (neighbor.region & 0xFFF) == region) ||
                ((neighbor.flags_7 & 2) == 0 &&
                    ((neighbor.region & 0xFFF) == 0 || (neighbor.region & 0x1000) == 0)))
            {
                discard_neighbor_distance(cell, neighbor);
                return false;
            }
        }
        if (direction > 7)
        {
            return true;
        }
    }
    while (true);
}
}
}
