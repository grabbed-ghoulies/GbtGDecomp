#include "grid_preparation.h"
#include "cell_registration.h"
#include "../entities/entity_heap.h"
#include "gbtg/anim.h"

#include <bit>
#include <cmath>
#include <cstring>

namespace gbtg::entity_detail
{
namespace
{
constexpr i32 neighbor_x[8] = {-1, 1, 0, 0, -1, 1, -1, 1};
constexpr i32 neighbor_z[8] = {0, 0, 1, -1, 1, 1, -1, -1};
}

// Xbox 0x00036380
const NavCell* next_navigation_neighbor(const NavGrid& grid, u32 source,
    i32 require_connection, i32 allow_blocked, i32 allow_terrain, u16 region,
    u32 surface_mask, std::span<const float> surface_costs, float& cost,
    i32& destination, i32& direction)
{
    const u32 width = u32(grid.width);
    const u32 column = source % width;
    const u32 row = source / width;
    const NavCell& first = grid.cells[source];
    while (direction < 8)
    {
        const i32 current = direction;
        const i32 x = std::bit_cast<i32>(column + u32(neighbor_x[current]));
        const i32 z = std::bit_cast<i32>(row + u32(neighbor_z[current]));
        ++direction;
        if (require_connection != 0 &&
            (first.connections & u8(1u << (u32(current) & 31))) == 0)
        {
            continue;
        }
        if (x < 0 || x >= grid.width || z < 0 || z >= grid.depth)
        {
            continue;
        }
        const i32 index = std::bit_cast<i32>(width * u32(z) + u32(x));
        const NavCell& next = grid.cells[index];
        if (allow_blocked == 0 &&
            !(region != 0 && (next.region & 0xFFF) == region) &&
            ((next.flags_7 & 2) != 0 ||
                ((next.region & 0xFFF) != 0 && (next.region & 0x1000) != 0)))
        {
            continue;
        }
        if (allow_terrain == 0 && (next.value_6 & 0x1F) != 0 &&
            (next.region & 0x8000) == 0 && (next.region & 0xFFF) == 0)
        {
            continue;
        }
        const u8 surface = u8(next.value_8 >> 24) != 0 ?
            u8(next.value_8 >> 24) : u8(next.value_8 >> 16);
        const float terrain_cost = (surface_mask & (1u << (surface & 31))) != 0 ?
            surface_costs[surface] : 1.f; // DAT_002FCFB8 / DAT_0032B0F8.
        const float stored_cost = float(u16(next.value_8)) * 0.0625f; // DAT_0032B1F8.
        const u16 flags = u16(next.value_6) | (u16(next.flags_7) << 8);
        const float multiplier = (float(((flags >> 5) & 15) + 1) * terrain_cost) * stored_cost;
        const float dx = float(i32(first.x) - i32(next.x));
        const float dy = float(i32(first.bottom) - i32(next.bottom));
        const float dz = float(i32(first.z) - i32(next.z));
        const float squared = dz * dz + (dy * dy + dx * dx);
        float distance = 0.f;
        if (squared != 0.f)
        {
            distance = float(retail_square_root(squared));
        }
        cost = distance * multiplier;
        destination = index;
        return &next;
    }
    return nullptr;
}

// Xbox 0x00111FC0
void accumulate_navigation_height_bounds(i32 floor, i32 count)
{
    // The retail four-cell unroll and its tail perform the same ordered writes.
    for (i32 index = 0; index < count; ++index)
    {
        const NavCell& cell = g_nav_grids[floor]->cells[index];
        if ((cell.flags_7 & 2) == 0)
        {
            float height = float(cell.bottom) - 5.f; // DAT_0032B184.
            float minimum = g_nav_min_y[floor];
            if (!(height > minimum)) // COMISS / JA accepts unordered.
            {
                minimum = height;
            }
            g_nav_min_y[floor] = minimum;
            const float maximum = g_nav_max_y[floor];
            if (!(maximum < height) && !std::isnan(maximum)) // COMISS / JC.
            {
                height = maximum;
            }
            g_nav_max_y[floor] = height;
        }
    }
}

// Xbox 0x001121B0
void initialize_navigation_edge_costs(i32 floor, i32 count,
    std::span<const float> surface_costs)
{
    NavGrid& grid = *g_nav_grids[floor];
    for (i32 index = 0; index < count; ++index)
    {
        NavCell& cell = grid.cells[index];
        if ((cell.flags_7 & 2) == 0)
        {
            i32 direction = 0;
            i32 destination;
            float cost;
            const NavCell* next;
            while ((next = next_navigation_neighbor(grid, u32(index), 0, 1, 1,
                0, 0xFFFFFFFFu, surface_costs, cost, destination, direction)) != nullptr)
            {
                if ((next->flags_7 & 2) != 0)
                {
                    cell.value_8 = (cell.value_8 & 0xFFFF0000u) | 0x20;
                    break;
                }
            }
        }
        const u32 x = u32(index) % u32(grid.width);
        const u32 z = u32(index) / u32(grid.width);
        if (std::bit_cast<i32>(x) < 1 || grid.width - 1 <= std::bit_cast<i32>(x) ||
            std::bit_cast<i32>(z) < 1 || grid.depth - 1 <= std::bit_cast<i32>(z))
        {
            cell.value_8 = (cell.value_8 & 0xFFFF0000u) | 0x20;
        }
        if ((cell.region & 0xFFF) != 0)
        {
            cell.value_8 = (cell.value_8 & 0xFFFF0000u) | 0x80;
        }
    }
}

// Xbox 0x00112560
void initialize_navigation_height_connections(i32 floor, i32 count,
    float height, std::span<const float> surface_costs)
{
    NavGrid& grid = *g_nav_grids[floor];
    for (i32 index = 0; index < count; ++index)
    {
        const float bottom = float(grid.cells[index].bottom);
        i32 direction = 0;
        i32 destination;
        float cost;
        u8 connections = 0;
        const NavCell* next = next_navigation_neighbor(grid, u32(index), 0, 1, 1,
            0, 0xFFFFFFFFu, surface_costs, cost, destination, direction);
        if (next != nullptr)
        {
            const float threshold = height * 1.5f; // DAT_0032B210.
            do
            {
                const float difference = float(next->bottom) - bottom;
                if (std::fabs(difference) < threshold)
                {
                    connections |= u8(1u << ((u32(direction) - 1) & 31));
                }
                next = next_navigation_neighbor(grid, u32(index), 0, 1, 1,
                    0, 0xFFFFFFFFu, surface_costs, cost, destination, direction);
            }
            while (next != nullptr);
        }
        grid.cells[index].connections = connections;
    }
}

// Xbox 0x001122E0
void mark_navigation_isolated_cells(i32 floor, i32 count)
{
    NavGrid& grid = *g_nav_grids[floor];
    for (i32 index = 0; index < count; ++index)
    {
        NavCell& cell = grid.cells[index];
        if ((cell.flags_7 & 2) != 0)
        {
            continue;
        }
        i32 direction = 0;
        const auto next_open = [&]() -> const NavCell*
        {
            while (direction < 8)
            {
                const i32 current = direction++;
                const i32 x = std::bit_cast<i32>(u32(index) % u32(grid.width) + u32(neighbor_x[current]));
                const i32 z = std::bit_cast<i32>(u32(index) / u32(grid.width) + u32(neighbor_z[current]));
                if (x < 0 || x >= grid.width || z < 0 || z >= grid.depth)
                {
                    continue;
                }
                const auto& next = grid.cells[u32(grid.width) * u32(z) + u32(x)];
                if ((next.flags_7 & 2) != 0 ||
                    ((next.region & 0xFFF) != 0 && (next.region & 0x1000) != 0) ||
                    ((next.value_6 & 0x1F) != 0 && (next.region & 0x8000) == 0 && (next.region & 0xFFF) == 0))
                {
                    continue;
                }
                float squared = 0.f;
                const float delta[3] = {float(i32(cell.x) - i32(next.x)),
                    float(i32(cell.bottom) - i32(next.bottom)), float(i32(cell.z) - i32(next.z))};
                for (float value : delta)
                {
                    squared = value * value + squared;
                }
                if (squared != 0.f)
                {
                    const volatile long double distance = retail_square_root(squared);
                    (void)distance;
                }
                return &next;
            }
            return nullptr;
        };
        const NavCell* next = next_open();
        if (next == nullptr || (direction > 4 && next_open() == nullptr))
        {
            cell.flags_7 |= 2;
        }
    }
}

// Xbox 0x00112680
void remove_navigation_connections(i32 floor, std::span<const NavigationConnectionPair> pairs)
{
    NavGrid& grid = *g_nav_grids[floor];
    for (const auto& pair : pairs)
    {
        const u8 first = grid.cells[pair.first].connections;
        const u8 second = grid.cells[pair.second].connections;
        const u32 first_x = pair.first % u32(grid.width);
        const u32 first_z = pair.first / u32(grid.width);
        const u32 second_x = pair.second % u32(grid.width);
        const u32 second_z = pair.second / u32(grid.width);
        u8 forward = 0;
        for (u32 direction = 0; direction < 8; ++direction)
        {
            if (u32(neighbor_x[direction]) == second_x - first_x &&
                u32(neighbor_z[direction]) == second_z - first_z)
            {
                forward = u8(1u << direction);
                break;
            }
        }
        u8 reverse = 0;
        for (u32 direction = 0; direction < 8; ++direction)
        {
            if (u32(neighbor_x[direction]) == first_x - second_x &&
                u32(neighbor_z[direction]) == first_z - second_z)
            {
                reverse = u8(1u << direction);
                break;
            }
        }
        grid.cells[pair.first].connections = u8(first & ~forward);
        grid.cells[pair.second].connections = u8(second & ~reverse);
    }
}

// Xbox 0x001127D0
void collect_navigation_floor_connections(i32 floor, i32 count)
{
    NavGrid& grid = *g_nav_grids[floor];
    for (i32 index = 0; index < count; ++index)
    {
        const auto& cell = grid.cells[index];
        if ((cell.flags_7 & 2) == 0 && (cell.region & 0xFFF) == 0 &&
            float(cell.bottom) > g_nav_min_y[floor + 1])
        {
            auto*& records = g_nav_floor_connections[floor];
            if (records == nullptr)
            {
                records = static_cast<NavigationCellRegistration*>(
                    engine_heap_allocate(i32(sizeof(NavigationCellRegistration)) + 0x30));
                records->begin = reinterpret_cast<u8*>(records + 1);
                records->end = records->begin;
                records->capacity = records->begin + 0x30;
                records->stride = 0xC;
                records->growth = 4;
            }
            if (records->end == records->capacity)
            {
                records = grow_navigation_cell_registration(records);
            }
            u8* row = records->end;
            records->end = row + records->stride;
            const u32 zero = 0;
            std::memcpy(row, &index, 4);
            std::memcpy(row + 4, &zero, 4);
        }
    }
}
}
