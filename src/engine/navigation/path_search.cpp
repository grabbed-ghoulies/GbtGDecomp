#include "floor_tracking.h"
#include "../audio/listener_settings.h"
#include "gbtg/anim.h"
#include <bit>
#include <cstring>

namespace gbtg::entity_detail
{
namespace
{
constexpr i32 neighbor_x[8] = {-1, 1, 0, 0, -1, 1, -1, 1};
constexpr i32 neighbor_z[8] = {0, 0, 1, -1, 1, 1, -1, -1};
// DAT_002FCFB8, eighteen retail surface multipliers.
constexpr float surface_costs[18] =
{
    1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 3, 1, 1, 1, 20, 1
};
template<class T>
T load(const u8* row, size_t offset)
{
    T value;
    std::memcpy(&value, row + offset, sizeof(value));
    return value;
}
template<class T>
void store(u8* row, size_t offset, T value)
{
    std::memcpy(row + offset, &value, sizeof(value));
}
u8* node(NavigationPathSearch& search, u32 reference)
{
    return search.nodes + (reference - 2) * 0x14;
}
u32 previous(NavigationPathSearch& search, u32 reference)
{
    return reference == 1 ? search.word_0 : load<u32>(node(search, reference), 0);
}
u32 next(NavigationPathSearch& search, u32 reference)
{
    return reference == 1 ? search.word_4 : load<u32>(node(search, reference), 4);
}
void set_previous(NavigationPathSearch& search, u32 reference, u32 value)
{
    if (reference == 1)
    {
        search.word_0 = value;
    }
    else
    {
        store(node(search, reference), 0, value);
    }
}
void set_next(NavigationPathSearch& search, u32 reference, u32 value)
{
    if (reference == 1)
    {
        search.word_4 = value;
    }
    else
    {
        store(node(search, reference), 4, value);
    }
}
void link_node(NavigationPathSearch& search, u32 reference)
{
    u32 head = 1;
    while (previous(search, head) != 0)
    {
        head = previous(search, head);
    }
    const u32 following = next(search, head);
    set_next(search, head, reference);
    set_previous(search, reference, head);
    set_next(search, reference, following);
    if (following != 0)
    {
        set_previous(search, following, reference);
    }
}
float distance(const NavCell& first, const NavCell& second)
{
    const float dx = float(i32(first.x) - i32(second.x));
    const float dy = float(i32(first.bottom) - i32(second.bottom));
    const float dz = float(i32(first.z) - i32(second.z));
    const float squared = dz * dz + (dy * dy + dx * dx);
    return squared != 0.0f ? float(retail_square_root(squared)) : 0.0f;
}
}

// Xbox 0x00036EB0
u32 advance_navigation_path_search(NavigationPathSearch& search, u32 surface_mask, float elapsed)
{
    const u32 target = u32(search.last) + 2;
    i32 allowed = 0;
    i32 expanded = 0;
    const float rate = std::bit_cast<float>(search.word_20);
    if (rate != 0.0f)
    {
        float remaining = std::bit_cast<float>(search.word_24) + elapsed;
        const float interval = 1.0f / rate;
        search.word_24 = std::bit_cast<u32>(remaining);
        if (!(remaining >= interval))
        {
            return 0;
        }
        do
        {
            remaining -= interval;
            ++allowed;
        }
        while (remaining >= interval);
        search.word_24 = std::bit_cast<u32>(remaining);
        if (allowed == 0)
        {
            return 0;
        }
    }
    const u16 region = search.grid->cells[search.first].region & 0xFFF;
    for (;;)
    {
        u32 best = search.word_4;
        if (best == 0)
        {
            return 1;
        }
        for (u32 candidate = next(search, best); candidate != 0; candidate = next(search, candidate))
        {
            if (load<u16>(node(search, candidate), 0x10) < load<u16>(node(search, best), 0x10))
            {
                best = candidate;
            }
        }
        if (best == 0)
        {
            return 1;
        }
        const u32 preceding = previous(search, best);
        const u32 following = next(search, best);
        if (preceding != 0)
        {
            set_next(search, preceding, following);
        }
        if (following != 0)
        {
            set_previous(search, following, preceding);
        }
        set_previous(search, best, 0);
        set_next(search, best, 0);
        if (best == target)
        {
            search.result_found = 1;
            return 1;
        }
        u8* const current = node(search, best);
        i32 direction = 0;
        for (;;)
        {
            const NavGrid& grid = *search.grid;
            const u32 cell_index = u32(i32(load<i16>(current, 0xA)));
            const i32 x = i32(cell_index % u32(grid.width));
            const i32 z = i32(cell_index / u32(grid.width));
            const NavCell& cell = grid.cells[cell_index];
            if (direction >= 8)
            {
                break;
            }
            while (direction < 8)
            {
                const i32 nx = std::bit_cast<i32>(u32(x) + u32(neighbor_x[direction]));
                const i32 nz = std::bit_cast<i32>(u32(z) + u32(neighbor_z[direction]));
                const u32 bit = 1u << direction;
                ++direction;
                if ((cell.connections & bit) == 0 || nx < 0 || nx >= grid.width || nz < 0 || nz >= grid.depth)
                {
                    continue;
                }
                const i32 destination = grid.width * nz + nx;
                const NavCell& adjacent = grid.cells[destination];
                if (!(region != 0 && (adjacent.region & 0xFFF) == region))
                {
                    if ((adjacent.flags_7 & 2) != 0 ||
                        ((adjacent.region & 0xFFF) != 0 && (adjacent.region & 0x1000) != 0))
                    {
                        continue;
                    }
                }
                if ((adjacent.value_6 & 0x1F) != 0 && (adjacent.region & 0x8000) == 0 &&
                    (adjacent.region & 0xFFF) == 0)
                {
                    continue;
                }
                const float base = float(u16(adjacent.value_8)) * 0.0625f;
                u8 surface = u8(adjacent.value_8 >> 24);
                if (surface == 0)
                {
                    surface = u8(adjacent.value_8 >> 16);
                }
                float surface_cost = 1.0f;
                if ((surface_mask & (1u << (surface & 31))) != 0)
                {
                    surface_cost = surface_costs[surface];
                }
                const u16 packed = u16(adjacent.value_6) | (u16(adjacent.flags_7) << 8);
                const float multiplier = (float(((packed >> 5) & 15) + 1) * surface_cost) * base;
                const float cost = float(load<u16>(current, 0xC)) * 0.25f + distance(cell, adjacent) * multiplier;
                u8* const destination_node = search.nodes + destination * 0x14;
                if ((load<u32>(destination_node, 0) == 0 && load<u16>(destination_node, 0x12) == 0) ||
                    !(cost >= float(load<u16>(destination_node, 0xC)) * 0.25f))
                {
                    const NavGrid& current_grid = *search.grid;
                    const float heuristic = current_grid.cost_scale * distance(
                        current_grid.cells[i32(load<i16>(destination_node, 0xA))], current_grid.cells[search.last]);
                    const long double total_cost = static_cast<long double>(cost) * 4.0L;
                    const float heuristic_cost = heuristic * 4.0f;
                    store(destination_node, 0xC, u16(truncate_runtime_float(total_cost)));
                    store(destination_node, 0xE, u16(truncate_runtime_float(heuristic_cost)));
                    store(destination_node, 0x10, u16(truncate_runtime_float(static_cast<long double>(heuristic_cost) + total_cost)));
                    store(destination_node, 8, load<u16>(current, 0xA));
                    store(destination_node, 0x12, u16(0));
                    if (load<u32>(destination_node, 0) == 0)
                    {
                        link_node(search, u32(destination) + 2);
                    }
                }
                goto next_neighbor;
            }
            break;
next_neighbor:
            ;
        }
        store(current, 0x12, u16(1));
        if (allowed != 0)
        {
            ++expanded;
            if (expanded >= allowed)
            {
                return 0;
            }
        }
    }
}

// Xbox 0x00036CA0
u32 update_navigation_path_search(NavigationPathSearch& search, u32 surface_mask, float elapsed)
{
    NavGrid& grid = *const_cast<NavGrid*>(search.grid);
    NavCell& target = grid.cells[search.last];
    const u8 occupancy = target.value_6 & 0x1F;
    const u8 blocked = target.flags_7 & 2;
    const u16 tagged = (target.region & 0xFFF) != 0 ? target.region & 0x1000 : 0;
    target.value_6 &= 0xE0;
    target.flags_7 &= 0xFD;
    target.region &= 0xEFFF;
    if (search.state == 1)
    {
        search.state = 2;
        search.word_24 = 0;
        search.result_found = 0;
        if (!navigation_cell_has_no_open_neighbor(grid, u32(search.first), 1) ||
            navigation_search_cells_connected(search, u32(search.last), search.first))
        {
            u8* const first = search.nodes + search.first * 0x14;
            store(first, 0xC, u16(0));
            store(first, 0x10, u16(0));
            store(first, 0xE, u16(truncate_runtime_float(navigation_cell_distance_cost(grid,
                u32(search.last), u32(search.first)))));
            store(first, 8, u16(0xFFFF));
            link_node(search, u32(search.first) + 2);
            if (advance_navigation_path_search(search, surface_mask, elapsed) != 0)
            {
                search.state = 3;
            }
        }
        else
        {
            search.state = 3;
        }
    }
    else if (search.state == 2)
    {
        if (advance_navigation_path_search(search, surface_mask, elapsed) != 0)
        {
            search.state = 3;
        }
    }
    NavCell& restored = const_cast<NavGrid*>(search.grid)->cells[search.last];
    restored.value_6 ^= (restored.value_6 ^ occupancy) & 0x1F;
    restored.flags_7 ^= (restored.flags_7 ^ blocked) & 2;
    restored.region ^= (restored.region ^ tagged) & 0x1000;
    return search.state;
}
}
