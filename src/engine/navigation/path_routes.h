#pragma once
#include "floor_tracking.h"
#include "../entities/ghouly_spawn_record.h"

namespace gbtg::entity_detail
{
float navigation_route_curve_parameter(const Vec3* points, float distance); // Xbox 0x00037F50
long double navigation_route_length(const GhoulySpawnPool& route); // Xbox 0x000377B0
void navigation_route_direction(const GhoulySpawnPool& route, Vec3& direction,
    Vec3* sampled, float distance); // Xbox 0x00037D50
u8* insert_navigation_route_row(GhoulySpawnPool*& route, i32 index); // Xbox 0x00066310
u32 retarget_navigation_search_route(NavigationPathSearch& search,
    i32 start, i32 finish); // Xbox 0x00037980
void append_navigation_route_point(GhoulySpawnPool*& route, i32 cell,
    const Vec3& point); // Xbox 0x000376B0
void set_navigation_route_endpoints(GhoulySpawnPool& route,
    const Vec3& start, const Vec3& finish); // Xbox 0x000376F0
u32 navigation_route_is_open(const GhoulySpawnPool& route,
    const NavigationPathSearch& search); // Xbox 0x00037720
u32 navigation_path_search_route_is_open(const NavigationPathSearch& search); // Xbox 0x00036E90
void build_navigation_search_route(const NavigationPathSearch& search,
    GhoulySpawnPool*& route, const Vec3& start, const Vec3& finish); // Xbox 0x00037470
GhoulySpawnPool* take_navigation_search_route(NavigationPathSearch& search,
    const Vec3& start, const Vec3& finish); // Xbox 0x00036E50
}
