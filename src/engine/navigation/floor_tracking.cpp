#include "floor_tracking.h"
#include "../entities/entity_heap.h"
#include <cstring>

namespace gbtg::entity_detail
{
// Xbox 0x00036AF0
void reset_navigation_path_search(NavigationPathSearch& search, u8* nodes, const NavGrid* grid,
    u32 count)
{
    if (search.scratch != nullptr)
    {
        engine_heap_free(search.scratch);
        search.scratch = nullptr;
    }
    if (nodes == nullptr)
    {
        nodes = search.nodes;
    }
    search = NavigationPathSearch{};
    search.grid = grid;
    search.first = -1;
    search.last = -1;
    search.nodes = nodes;
    search.count = count;
    for (u32 index = 0; index < count; ++index)
    {
        const u16 value = u16(index);
        std::memcpy(search.nodes + size_t(index) * 0x14 + 0xA, &value, 2);
    }
    search.state = 0;
}

// Xbox 0x00036BC0
void clear_navigation_path_search(NavigationPathSearch& search)
{
    for (u32 index = 0; index < search.count; ++index)
    {
        u8* const row = search.nodes + size_t(index) * 0x14;
        std::memset(row, 0, 0x14);
        const u16 no_index = 0xFFFF;
        std::memcpy(row + 8, &no_index, sizeof(no_index));
    }
    search.first = -1;
    search.last = -1;
    search.word_0 = 0;
    search.word_4 = 0;
    if (search.scratch != nullptr)
    {
        engine_heap_free(search.scratch);
        search.scratch = nullptr;
    }
    search.word_30 = 0;
    search.state = 0;
}

// Xbox 0x00036C80
void begin_navigation_path_search(NavigationPathSearch& search, i32 last, i32 first)
{
    clear_navigation_path_search(search);
    search.last = last;
    search.first = first;
    search.state = 1;
}

// Xbox 0x00036C60
u32 navigation_path_search_has_result(const NavigationPathSearch& search)
{
    return search.state == 3 && search.result_found != 0 ? 1 : 0;
}

// Xbox 0x00110C90
u32 update_navigation_floor(const Vec3& point, i32& floor, NavigationCellRegistration*& records,
    NavigationPathSearch* search, i32 preferred, i32 check_cell)
{
    const i32 found = navigation_floor_index(point, preferred, check_cell);
    const u32 changed = found != floor;
    if (changed == 0)
    {
        floor = found;
        return 0;
    }
    release_navigation_cell_registration(records, u32(floor));
    if (search != nullptr)
    {
        const NavGrid* grid = g_nav_grids[found];
        reset_navigation_path_search(*search, nullptr, grid, u32(grid->depth) * u32(grid->width));
    }
    floor = found;
    return changed;
}
}
