#pragma once
#include "navigation.h"

namespace gbtg::entity_detail
{
i32 set_navigation_region_blocked(NavGrid& grid, u16 tag,
    i16 blocked); // Xbox 0x00036A00
void set_all_navigation_regions_blocked(u16 tag, i16 blocked); // Xbox 0x001117E0
}
