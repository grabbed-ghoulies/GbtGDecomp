#pragma once
#include "navigation.h"

namespace gbtg::entity_detail
{
// Heap header followed by its rows. Retail packs stride +0, growth +2, begin +4, end +8
// and capacity +0xC in a 0x10-byte header; rows hold the cell index at +0 and the
// counter selector at +4.
struct NavigationCellRegistration
{
    u8* begin; // Retail header +4.
    u8* end; // Retail header +8; rows have fixed 0xC stride.
    u8* capacity = nullptr; // Retail header +0xC.
    u16 stride = 0; // Retail header +0.
    u16 growth = 0; // Retail header +2.
};
extern NavigationCellRegistration* g_nav_floor_connections[kNavSlots]; // DAT_0050E248.
NavigationCellRegistration* grow_navigation_cell_registration(
    NavigationCellRegistration* records); // Xbox 0x000662A0
u32 navigation_floor_connection_point(i32 source_floor, i32 target_floor,
    const Vec3& target, Vec3& output); // Xbox 0x00110D00
void decrement_navigation_cell_counts(const NavigationCellRegistration& records,
    u32 grid); // Xbox 0x00110470
void increment_navigation_cell_counts(const NavigationCellRegistration& records,
    u32 grid); // Xbox 0x00110400
void release_navigation_cell_registration(NavigationCellRegistration*& records,
    u32 grid); // Xbox 0x001108B0
void clamped_navigation_cell_coordinates(const NavGrid& grid, const Vec3& point, i32& x,
    i32& z); // Xbox 0x00110970
u32 navigation_cell_bounds(i32 floor, const Vec3* corners, i32& min_x, i32& max_x, i32& min_z,
    i32& max_z); // Xbox 0x001109E0
void register_navigation_cells(NavigationCellRegistration*& records, const Vec3& center, i32 floor,
    float radius, i32 include_outer); // Xbox 0x001104E0
}
