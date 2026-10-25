#include "occupancy.h"
#include "gbtg/anim.h"
#include "../entities/entity.h"

#include <cmath>
#include <cstddef>

namespace gbtg
{
static_assert(sizeof(OccupancyRecord) == 0x38);
static_assert(offsetof(OccupancyRecord, mask) == 4);
static_assert(offsetof(OccupancyRecord, kind) == 8);
static_assert(offsetof(OccupancyRecord, group) == 0x1C);
static_assert(offsetof(OccupancyRecord, center) == 0x20);
static_assert(offsetof(OccupancyRecord, extent_x) == 0x2C);
static_assert(offsetof(OccupancyRecord, extent_z) == 0x30);
static_assert(offsetof(OccupancyRecord, yaw) == 0x34);

// Xbox 0x00124640
bool occupancy_sphere_contains(const OccupancyRecord& record, const Vec3& point, float& distance)
{
    const long double dx = static_cast<long double>(point.x) - record.center.x;
    const long double dy = static_cast<long double>(point.y) - record.center.y;
    const long double dz = static_cast<long double>(point.z) - record.center.z;
    const float outside = float(std::sqrt(dx * dx + dy * dy + dz * dz) - record.extent_x);
    if (outside > 0.f)
    {
        distance = outside;
        return false;
    }
    return true;
}

// Xbox 0x001244D0
bool occupancy_box_contains(const OccupancyRecord& record, const Vec3& point, float& distance)
{
    float x = point.x - record.center.x;
    float z = point.z - record.center.z;
    const float height = engine_absolute(point.y - record.center.y);
    if (height > record.extent_x && height > record.extent_z)
    {
        distance = height;
        return false;
    }
    if (record.yaw != 0.f)
    {
        const float sine = float(retail_sin(-record.yaw));
        const float cosine = float(retail_cos(-record.yaw));
        const float rotated_x = sine * z + cosine * x;
        z = float(static_cast<long double>(cosine) * z - static_cast<long double>(sine) * x);
        x = rotated_x;
    }
    float low_x = (0.f - record.extent_x) - x;
    float high_x = x - record.extent_x;
    float low_z = (0.f - record.extent_z) - z;
    const float high_z = z - record.extent_z;
    // Each retail COMISS/JA rejects only an ordered positive value.
    if (!(low_x > 0.f) && !(high_x > 0.f) && !(low_z > 0.f) && !(high_z > 0.f))
    {
        return true;
    }
    if (!(low_x >= 0.f))
    {
        low_x = 0.f;
    }
    if (!(high_x >= low_x))
    {
        high_x = low_x;
    }
    if (!(low_z >= 0.f))
    {
        low_z = 0.f;
    }
    if (high_z >= low_z)
    {
        low_z = high_z;
    }
    distance = float(std::sqrt(double(low_z * low_z + high_x * high_x)));
    return false;
}

// Xbox 0x00124860. EDI is the point. Each DAT_00512344 record whose dword at +0 is the region tests the point with
// its kind's shape (0 sphere, 1 box); the first that contains it returns 1.
// Xbox 0x00124860
bool occupancy_region_contains(u32 region, const Vec3& point)
{
    if (g_512344 == nullptr)
    {
        return false;
    }
    const auto& pool = *static_cast<const OccupancyPool*>(g_512344);
    float distance = 0.f;
    for (const OccupancyRecord* record = pool.begin; record < pool.cursor; ++record)
    {
        if (record->value_0 != region)
        {
            continue;
        }
        bool contains = false;
        if (record->kind == 0)
        {
            contains = occupancy_sphere_contains(*record, point, distance);
        }
        else if (record->kind == 1)
        {
            contains = occupancy_box_contains(*record, point, distance);
        }
        if (contains)
        {
            return true;
        }
    }
    return false;
}

// Xbox 0x001248D0
const OccupancyRecord* find_occupancy_record(const Vec3& point, float initial_distance,
    u32 mask, const OccupancyRecord* excluded, i16 group)
{
    if (g_512344 == nullptr || (u32(g_512348) & mask) == 0)
    {
        return nullptr;
    }
    const auto& pool = *static_cast<const OccupancyPool*>(g_512344);
    const OccupancyRecord* record = pool.begin;
    float distance = initial_distance;
    do
    {
        if (record >= pool.cursor)
        {
            return nullptr;
        }
        if ((record->mask & mask) != 0 && record != excluded &&
            (group == 0 || record->group == 0 || record->group == group))
        {
            bool contains;
            if (record->kind == 0)
            {
                contains = occupancy_sphere_contains(*record, point, distance);
            }
            else
            {
                if (record->kind != 1)
                {
                    ++record;
                    continue;
                }
                contains = occupancy_box_contains(*record, point, distance);
            }
            if (contains)
            {
                return record;
            }
        }
        ++record;
    }
    while (true);
}

// Xbox 0x001249B0
void clear_occupancy_value_mask(u32 value, u32 mask)
{
    auto& pool = *static_cast<OccupancyPool*>(g_512344);
    for (OccupancyRecord* entry = pool.begin; entry < pool.cursor; ++entry)
    {
        if (entry->value_0 == value)
        {
            entry->mask &= ~mask;
        }
    }
    g_512348 = 0;
    for (const OccupancyRecord* entry = pool.begin; entry < pool.cursor; ++entry)
    {
        g_512348 = i32(u32(g_512348) | entry->mask);
    }
}

// Xbox 0x00124970
void clear_occupancy_record_mask(OccupancyRecord& record, u32 mask)
{
    record.mask &= ~mask;
    g_512348 = 0;
    const auto& pool = *static_cast<const OccupancyPool*>(g_512344);
    for (const OccupancyRecord* entry = pool.begin; entry < pool.cursor; ++entry)
    {
        g_512348 = i32(u32(g_512348) | entry->mask);
    }
}
}
