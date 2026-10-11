#pragma once
#include "navigation.h"
#include "../entities/ghouly_spawn_record.h"

namespace gbtg::entity_detail
{
GhoulySpawnPool* collect_navigation_spawn_candidates(i32 floor, const Vec3* origin,
    const Vec3& center, float minimum, float maximum,
    const OccupancyRecord* required, const OccupancyRecord* allowed,
    u8 flags, i16 group, float occupancy_initial); // Xbox 0x0010F7F0
bool select_navigation_spawn_position(i32 floor, const Vec3* origin,
    const Vec3& center, float minimum, float maximum,
    const OccupancyRecord* required, const OccupancyRecord* allowed,
    u8 flags, i16 group, Vec3& output, u32& random, float occupancy_initial); // Xbox 0x00111420
}

