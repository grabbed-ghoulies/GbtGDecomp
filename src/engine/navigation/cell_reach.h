#pragma once
#include "navigation.h"
#include <vector>

namespace gbtg
{
// One entry of the heap cell list 0x0010FE10 builds (stride 0xC): cell index +0, zero +4.
struct RuntimeNavigationCellEntry
{
    u32 cell;
    u32 value;
};
// Cells of the floor's grid inside the square around point, kept when within radius plus half of grid 0's
// cell size and outside inner; empty when the grid is absent or no cell qualifies (retail returns 0).
std::vector<RuntimeNavigationCellEntry> collect_navigation_cells_near(i32 floor, const Vec3& point, float inner,
    float radius); // Xbox 0x0010FE10, EAX floor
// Whether any cell under the target record's footprint has a clear path to the eye; no cells counts as clear.
u32 navigation_record_reaches_point(std::span<const u8, 0x20> record, const Vec3& eye, i32 region_mode,
    i32 allow_tagged, i32 reject_marked); // Xbox 0x0012CEF0, EAX record, EBX eye
}
