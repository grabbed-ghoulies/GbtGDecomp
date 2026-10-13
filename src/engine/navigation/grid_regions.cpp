#include "grid_regions.h"

#include <bit>

namespace gbtg::entity_detail
{
// Xbox 0x001117E0
void set_all_navigation_regions_blocked(u16 tag, i16 blocked)
{
    if (g_nav_count != 0)
    {
        i32 index = 0;
        if (g_nav_count > 0)
        {
            do
            {
                set_navigation_region_blocked(*g_nav_grids[index], tag, blocked);
                ++index;
            }
            while (index < g_nav_count);
        }
    }
}

// Xbox 0x00036A00
i32 set_navigation_region_blocked(NavGrid& grid, u16 tag, i16 blocked)
{
    i32 index = 0;
    i32 changed = 0;
    const i32 count = std::bit_cast<i32>(u32(grid.width) * u32(grid.depth));
    if (count > 0)
    {
        do
        {
            const u16 old = grid.cells[index].region;
            if ((old & 0xFFF) == tag)
            {
                grid.cells[index].region = u16(((u32(u16(blocked)) << 12) ^ old) & 0x1000) ^ old;
                ++changed;
            }
            ++index;
        }
        while (index < count);
    }
    return changed;
}
}
