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
// The cells of the floor inside the box from `low` to `high` (x and z) that a spawn may use, as {cell, 0} rows.
GhoulySpawnPool* collect_navigation_box_cells(i32 floor, const Vec3& high,
    const Vec3& low); // Xbox 0x0010FBE0, EAX floor, ECX high, EDX low
// The point moved onto its floor's cell bottom; false when no cell holds it.
bool settle_navigation_point(i32 floor, Vec3& point); // Xbox 0x00111B40, EAX point, ESI floor
// A random free point in an occupancy record (actor +0xCA0's room): kind 0 within its radius, kind 1 inside its box.
bool select_occupancy_record_point(const OccupancyRecord& record, Vec3& output, u32& random); // Xbox 0x00110F50
}
