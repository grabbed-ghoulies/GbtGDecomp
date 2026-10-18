#pragma once

#include "gbtg/types.h"
#include "occupancy.h"
#include <vector>

namespace gbtg
{
namespace entity_detail
{
inline constexpr i32 kNavSlots = 4;
inline constexpr i32 kNavBands = 2;
#pragma pack(push, 2)
struct NavCell
{
    i16 x = 0;             // +0
    i16 bottom = 0;        // +2
    i16 z = 0;             // +4
    u8 value_6 = 0;
    u8 flags_7 = 0;
    u32 value_8 = 0;
    u16 region = 0;        // +0xC
    i16 top = 0;           // +0xE
    u8 connections = 0;    // +0x10
    u8 flags_11 = 0;
};
#pragma pack(pop)
struct NavGrid
{
    i32 width = 0; // +0
    i32 depth = 0; // +4
    i32 cell = 0;  // +8
    float cost_scale = 0.0f; // +0xC, retail path distance multiplier.
    std::vector<NavCell> cells; // +0x10, stride 0x12
};
struct NavigationPathSearch;
float navigation_cell_distance_cost(const NavGrid& grid, u32 source,
    u32 destination); // Xbox 0x00036A60
bool navigation_search_cells_connected(const NavigationPathSearch& search,
    u32 source, i32 destination); // Xbox 0x00037340
i32 navigation_nearest_open_cell(u32 cell, i32 floor,
    Vec3* point); // Xbox 0x00111660
extern NavGrid* g_nav_grids[kNavSlots];
extern i32 g_nav_count;
extern float g_nav_min_y[kNavBands];
extern float g_nav_max_y[kNavBands];
extern i32 g_nav_origin_x;
extern i32 g_nav_origin_z;
i32 navigation_floor_index(const Vec3& point, i32 preferred, i32 check_cell); // Xbox 0x00110BF0
i32 navigation_cell_index(i32 floor, const Vec3& point); // Xbox 0x00110EF0
// The floor cell under the point and its +2 height; false outside the grid.
bool navigation_cell_height(i32 floor, const Vec3& point, float& height); // Xbox 0x00110BB0, ESI floor, EBX point
u8 navigation_surface_kind(i32 floor, const Vec3& point); // Xbox 0x001117A0
bool navigation_path_clear(const Vec3& source, const Vec3& destination, i32 region_mode,
    i32 allow_tagged, i32 query_occupancy, i32 reject_marked, const OccupancyRecord* allowed,
    i32* last_cell, i16 group); // Xbox 0x0012CC50
bool navigation_cell_allowed(const NavGrid& grid, u32 index, i32 reject_marked,
    i32 region_mode, i32 allow_tagged, i32 query_occupancy, const OccupancyRecord* allowed,
    i16 group); // Xbox 0x0012CB80
bool navigation_cell_coordinates(i32 floor, const Vec3& point, i32& x, i32& z); // Xbox 0x00110E80
bool navigation_cells_connected(const NavGrid& grid, u32 destination, i32 source); // Xbox 0x0012CA40
bool navigation_cell_is_isolated(const NavGrid& grid, u32 cell, bool use_region); // Xbox 0x00036770
u32 navigation_neighbor_direction_mask(i32 x, u32 z); // Xbox 0x00036560
bool navigation_neighbor_connected(const NavGrid& grid, u32 source,
    u32 destination); // Xbox 0x00036590
bool navigation_cell_has_no_open_neighbor(const NavGrid& grid, u32 cell,
    u32 use_region); // Xbox 0x00036600
bool navigation_cell_has_no_unblocked_neighbor(const NavGrid& grid,
    u32 cell); // Xbox 0x000368C0
}
}
