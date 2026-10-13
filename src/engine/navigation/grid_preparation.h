#pragma once

#include "navigation.h"
#include <span>

namespace gbtg::entity_detail
{
const NavCell* next_navigation_neighbor(const NavGrid& grid, u32 source,
    i32 require_connection, i32 allow_blocked, i32 allow_terrain, u16 region,
    u32 surface_mask, std::span<const float> surface_costs, float& cost,
    i32& destination, i32& direction); // Xbox 0x00036380
void accumulate_navigation_height_bounds(i32 floor, i32 count); // Xbox 0x00111FC0
void initialize_navigation_edge_costs(i32 floor, i32 count,
    std::span<const float> surface_costs); // Xbox 0x001121B0
void initialize_navigation_height_connections(i32 floor, i32 count,
    float height, std::span<const float> surface_costs); // Xbox 0x00112560
void mark_navigation_isolated_cells(i32 floor, i32 count); // Xbox 0x001122E0
struct NavigationConnectionPair
{
    u32 first;
    u32 second;
};
void remove_navigation_connections(i32 floor,
    std::span<const NavigationConnectionPair> pairs); // Xbox 0x00112680
void collect_navigation_floor_connections(i32 floor, i32 count); // Xbox 0x001127D0
}
