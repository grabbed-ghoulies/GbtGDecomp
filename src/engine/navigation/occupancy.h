#pragma once

#include "gbtg/types.h"

namespace gbtg
{
struct OccupancyRecord
{
    u32 value_0 = 0;
    u32 mask = 0; // +4
    i16 kind = 0; // +8: 0 sphere, 1 rotated box
    u8 values_a[0x12]{};
    i16 group = 0; // +0x1C
    u16 value_1e = 0;
    Vec3 center{}; // +0x20
    float extent_x = 0.f; // +0x2C: sphere radius or box X extent
    float extent_z = 0.f; // +0x30
    float yaw = 0.f;      // +0x34
};
struct OccupancyPool
{
    u16 stride;
    u16 grow;
    OccupancyRecord* begin; // +4
    OccupancyRecord* cursor; // +8: used end
    OccupancyRecord* limit; // +0xC
};
extern void* g_512344;
extern i32 g_512348;
bool occupancy_sphere_contains(const OccupancyRecord& record, const Vec3& point,
    float& distance); // Xbox 0x00124640
bool occupancy_box_contains(const OccupancyRecord& record, const Vec3& point,
    float& distance); // Xbox 0x001244D0
bool occupancy_region_contains(u32 region, const Vec3& point); // Xbox 0x00124860, EDI point
const OccupancyRecord* find_occupancy_record(const Vec3& point, float initial_distance,
    u32 mask, const OccupancyRecord* excluded, i16 group); // Xbox 0x001248D0
// ECX record, EAX mask: clears the mask bits on the record, then rebuilds DAT_00512348 from every record's mask.
void clear_occupancy_record_mask(OccupancyRecord& record, u32 mask); // Xbox 0x00124970
// EDI value, EAX mask: clears the mask bits on every record whose +0 is the value, then rebuilds DAT_00512348.
void clear_occupancy_value_mask(u32 value, u32 mask); // Xbox 0x001249B0
}
