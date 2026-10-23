#include "spawn_candidates.h"
#include "cell_registration.h"
#include "../entities/entity_heap.h"
#include "../entities/ghouly_box_decoder.h"
#include "gbtg/anim.h"
#include <bit>
#include <cstring>
#include <xmmintrin.h>

namespace gbtg::entity_detail
{
// Xbox 0x0010F7F0
GhoulySpawnPool* collect_navigation_spawn_candidates(i32 floor, const Vec3* origin,
    const Vec3& center, float minimum, float maximum,
    const OccupancyRecord* required, const OccupancyRecord* allowed,
    u8 flags, i16 group, float occupancy_initial)
{
    auto* grid = g_nav_grids[floor];
    if (grid == nullptr)
    {
        return nullptr;
    }
    if (origin == nullptr)
    {
        origin = &center;
    }
    const Vec3 corners[4]{{center.x - maximum, center.y, center.z - maximum},
        {center.x + maximum, center.y, center.z - maximum},
        {center.x + maximum, center.y, center.z + maximum},
        {center.x - maximum, center.y, center.z + maximum}};
    i32 min_x, max_x, min_z, max_z;
    navigation_cell_bounds(floor, corners, min_x, max_x, min_z, max_z);
    const float squared_minimum = minimum * minimum;
    float half_cell;
    if (g_nav_grids[0] == nullptr)
    {
        half_cell = 0.f;
    }
    else
    {
        half_cell = float(g_nav_grids[0]->cell) * std::bit_cast<float>(0x3F000000u);
    }
    const float outer = half_cell + maximum;
    const float squared_maximum = outer * outer;
    GhoulySpawnPool* pool = nullptr;
    for (i32 x = min_x; x <= max_x; ++x)
    {
        for (i32 z = min_z; z <= max_z; ++z)
        {
            const i32 index = grid->width * z + x;
            const NavCell& cell = grid->cells[index];
            const u16 cell_flags = u16(cell.value_6) | (u16(cell.flags_7) << 8);
            if ((cell_flags & 0x200) != 0 || (cell_flags & 0x1F) != 0 ||
                ((cell.region & 0xFFF) != 0 && (cell.region & 0x1000) != 0))
            {
                continue;
            }
            if ((flags & 1) != 0 && float(u16(cell.value_8)) * std::bit_cast<float>(0x3D800000u) > 1.f)
            {
                continue;
            }
            if ((flags & 2) != 0 && (cell.flags_7 & 8) != 0)
            {
                continue;
            }
            if ((flags & 8) != 0 && (cell.region & 0xFFF) != 0)
            {
                continue;
            }
            const Vec3 point{float(cell.x), float(cell.bottom), float(cell.z)};
            float distance = occupancy_initial;
            if ((flags & 0x10) != 0)
            {
                const bool occupied = (flags & 4) != 0;
                i32 last_cell;
                if (!navigation_path_clear(point, *origin, 1, 0, occupied ? 1 : 0,
                    0, occupied ? allowed : nullptr, &last_cell, 0))
                {
                    continue;
                }
            }
            if ((flags & 4) != 0)
            {
                const auto* record = find_occupancy_record(point, distance, 4, nullptr, group);
                if (record != nullptr && record != allowed)
                {
                    continue;
                }
            }
            if (maximum > 0.f || minimum > 0.f)
            {
                const float dx = point.x - center.x;
                const float dz = point.z - center.z;
                const float squared = dz * dz + dx * dx;
                if (maximum > 0.f && squared > squared_maximum)
                {
                    continue;
                }
                if (minimum > 0.f && squared_minimum > squared)
                {
                    continue;
                }
            }
            if (required != nullptr)
            {
                if (required->kind == 0)
                {
                    if (!occupancy_sphere_contains(*required, point, distance))
                    {
                        continue;
                    }
                }
                else
                {
                    if (required->kind != 1)
                    {
                        continue;
                    }
                    if (!occupancy_box_contains(*required, point, distance))
                    {
                        continue;
                    }
                }
            }
            if (pool == nullptr)
            {
                pool = static_cast<GhoulySpawnPool*>(engine_heap_allocate(i32(sizeof(GhoulySpawnPool) + 0x30)));
                pool->begin = reinterpret_cast<u8*>(pool + 1);
                pool->cursor = pool->begin;
                pool->stride = 0xC;
                pool->grow = 4;
                pool->limit = pool->begin + 0x30;
            }
            if (pool->cursor == pool->limit)
            {
                pool = grow_ghouly_spawn_pool(pool);
            }
            u8* row = pool->cursor;
            pool->cursor += pool->stride;
            std::memcpy(row, &index, 4);
            const u32 zero = 0;
            std::memcpy(row + 4, &zero, 4);
        }
    }
    return pool;
}

// Xbox 0x00111420
bool select_navigation_spawn_position(i32 floor, const Vec3* origin,
    const Vec3& center, float minimum, float maximum,
    const OccupancyRecord* required, const OccupancyRecord* allowed,
    u8 flags, i16 group, Vec3& output, u32& random, float occupancy_initial)
{
    auto* pool = collect_navigation_spawn_candidates(floor, origin, center, minimum,
        maximum, required, allowed, u8(flags | 1), group, occupancy_initial);
    if (pool == nullptr)
    {
        if ((flags & 1) != 0)
        {
            return false;
        }
        pool = collect_navigation_spawn_candidates(floor, origin, center, minimum,
            maximum, required, allowed, flags, group, occupancy_initial);
        if (pool == nullptr)
        {
            return false;
        }
    }
    const i32 count = i32(pool->cursor - pool->begin) / pool->stride;
    random = random * 0x19660Du + 0x3C6EF35Fu;
    const float fraction = std::bit_cast<float>((random & 0x7FFFFF) | 0x3F800000u) - 1.f;
    const i32 chosen = _mm_cvtt_ss2si(_mm_set_ss(fraction * float(count)));
    const i32 index = std::bit_cast<i32>(read_u32({pool->begin + chosen * 0xC, 0xC}, 0));
    const NavCell& selected = g_nav_grids[floor]->cells[index];
    output = {float(selected.x), float(selected.bottom), float(selected.z)};
    engine_heap_free(pool);
    float half_cell;
    if (g_nav_grids[0] == nullptr)
    {
        half_cell = 0.f;
    }
    else
    {
        half_cell = float(g_nav_grids[0]->cell) * std::bit_cast<float>(0x3F000000u);
    }
    const float jitter = half_cell * std::bit_cast<float>(0x3F733333u);
    const float low = 0.f - jitter;
    random = random * 0x19660Du + 0x3C6EF35Fu;
    const float x_fraction = std::bit_cast<float>((random & 0x7FFFFF) | 0x3F800000u) - 1.f;
    output.x = ((jitter - low) * x_fraction + output.x) + low;
    random = random * 0x19660Du + 0x3C6EF35Fu;
    const float z_fraction = std::bit_cast<float>((random & 0x7FFFFF) | 0x3F800000u) - 1.f;
    output.z = ((jitter - low) * z_fraction + output.z) + low;
    const i32 result = navigation_cell_index(floor, output);
    if (result == -1)
    {
        return false;
    }
    output.y = float(g_nav_grids[floor]->cells[result].bottom);
    return true;
}
namespace
{
// Xbox 0x00106880, ESI input, EDI output
void rotate_runtime_yaw(const Vec3& input, Vec3& output, float yaw)
{
    const float sine = float(retail_sin(yaw)); // 0x00153320
    const float cosine = float(retail_cos(yaw)); // 0x00153230
    const Vec3 source = input;
    output.y = source.y;
    output.x = source.x * cosine + sine * source.z;
    output.z = cosine * source.z - sine * source.x;
}

float half_navigation_cell()
{
    if (g_nav_grids[0] == nullptr)
    {
        return 0.f;
    }
    return float(g_nav_grids[0]->cell) * 0.5f; // DAT_0032B0F4
}

float next_random_fraction(u32& random)
{
    random = random * 0x19660Du + 0x3C6EF35Fu;
    return std::bit_cast<float>((random & 0x7FFFFF) | 0x3F800000u) - 1.f; // DAT_0032B0F8
}
}

// Xbox 0x0010FBE0, EAX floor, ECX high, EDX low
GhoulySpawnPool* collect_navigation_box_cells(i32 floor, const Vec3& high, const Vec3& low)
{
    auto* grid = g_nav_grids[floor];
    if (grid == nullptr)
    {
        return nullptr;
    }
    const Vec3 corners[4]{{low.x, low.y, low.z}, {low.x, low.y, high.z}, {high.x, high.y, high.z},
        {high.x, high.y, low.z}};
    i32 min_x, max_x, min_z, max_z;
    navigation_cell_bounds(floor, corners, min_x, max_x, min_z, max_z); // 0x001109E0
    GhoulySpawnPool* pool = nullptr;
    for (i32 x = min_x; x <= max_x; ++x)
    {
        for (i32 z = min_z; z <= max_z; ++z)
        {
            const i32 index = grid->width * z + x;
            const NavCell& cell = grid->cells[index];
            const u16 cell_flags = u16(cell.value_6) | (u16(cell.flags_7) << 8);
            if ((cell_flags & 0x200) != 0 || (cell_flags & 0x1F) != 0 ||
                ((cell.region & 0xFFF) != 0 && (cell.region & 0x1000) != 0))
            {
                continue;
            }
            if (pool == nullptr)
            {
                pool = static_cast<GhoulySpawnPool*>(engine_heap_allocate(i32(sizeof(GhoulySpawnPool) + 0x30)));
                pool->begin = reinterpret_cast<u8*>(pool + 1);
                pool->cursor = pool->begin;
                pool->stride = 0xC;
                pool->grow = 4;
                pool->limit = pool->begin + 0x30;
            }
            if (pool->cursor == pool->limit)
            {
                pool = grow_ghouly_spawn_pool(pool); // The inlined 0x000F73A0 growth.
            }
            u8* row = pool->cursor;
            pool->cursor += pool->stride;
            std::memcpy(row, &index, 4);
            const u32 zero = 0;
            std::memcpy(row + 4, &zero, 4);
        }
    }
    return pool;
}

// Xbox 0x00111B40, EAX point, ESI floor
bool settle_navigation_point(i32 floor, Vec3& point)
{
    const i32 cell = navigation_cell_index(floor, point); // 0x00110EF0
    if (cell == -1)
    {
        return false;
    }
    point.y = float(g_nav_grids[floor]->cells[cell].bottom);
    return true;
}

// Xbox 0x00110F50
bool select_occupancy_record_point(const OccupancyRecord& record, Vec3& output, u32& random)
{
    if (record.kind == 0)
    {
        const i32 floor = navigation_floor_index(record.center, -1, 0); // 0x00110BF0
        // Flags 0 and no required record: the occupancy distance is never read.
        return select_navigation_spawn_position(floor, nullptr, record.center, 0.f, record.extent_x, nullptr,
            nullptr, 0, 0, output, random, 0.f); // 0x00111420
    }
    if (record.kind != 1)
    {
        return false;
    }
    const bool rotated = record.yaw != 0.f;
    float min_x;
    float min_z;
    float max_x;
    float max_z;
    if (rotated)
    {
        const float extent = record.extent_x;
        Vec3 a{-extent, 0.f, -extent};
        Vec3 b{-extent, 0.f, extent};
        Vec3 c{extent, 0.f, -extent};
        Vec3 d{extent, 0.f, extent};
        rotate_runtime_yaw(a, a, record.yaw);
        rotate_runtime_yaw(b, b, record.yaw);
        rotate_runtime_yaw(c, c, record.yaw);
        rotate_runtime_yaw(d, d, record.yaw);
        // Each COMISS keeps the earlier value unless the later one is strictly beyond it.
        min_x = d.x > c.x ? c.x : d.x;
        if (min_x > b.x)
        {
            min_x = b.x;
        }
        if (min_x > a.x)
        {
            min_x = a.x;
        }
        min_z = d.z > c.z ? c.z : d.z;
        if (min_z > b.z)
        {
            min_z = b.z;
        }
        if (min_z > a.z)
        {
            min_z = a.z;
        }
        max_x = c.x < d.x ? d.x : c.x;
        if (!(b.x < max_x))
        {
            max_x = b.x;
        }
        if (!(a.x < max_x))
        {
            max_x = a.x;
        }
        max_z = c.z < d.z ? d.z : c.z;
        if (!(b.z < max_z))
        {
            max_z = b.z;
        }
        if (!(a.z < max_z))
        {
            max_z = a.z;
        }
    }
    else
    {
        max_x = record.extent_x;
        max_z = record.extent_z;
        min_x = 0.f - record.extent_x;
        min_z = 0.f - record.extent_z;
    }
    const Vec3 low{record.center.x + min_x, record.center.y, record.center.z + min_z};
    const Vec3 high{record.center.x + max_x, record.center.y, record.center.z + max_z};
    const i32 floor = navigation_floor_index(record.center, -1, 0); // 0x00110BF0
    GhoulySpawnPool* pool = collect_navigation_box_cells(floor, high, low);
    if (pool == nullptr)
    {
        return false;
    }
    if (rotated)
    {
        // Drop the cells outside the turned box by more than three quarters of a half cell.
        const float slack = half_navigation_cell() * 0.75f; // DAT_0032B2AC
        u8* row = pool->begin;
        u8* end = pool->cursor;
        while (row < end)
        {
            i32 index;
            std::memcpy(&index, row, 4);
            const NavCell& cell = g_nav_grids[floor]->cells[index];
            const Vec3 point{float(cell.x), float(cell.bottom), float(cell.z)};
            float distance = 0.f;
            if (occupancy_box_contains(record, point, distance) || !(distance > slack)) // 0x001244D0
            {
                row += 0xC;
                continue;
            }
            u8* const last = pool->cursor - pool->stride;
            if (row < last)
            {
                std::memmove(row, row + pool->stride, size_t(last - row)); // 0x00165AF0
            }
            end -= 0xC;
            pool->cursor -= pool->stride;
        }
    }
    const i32 count = i32(pool->cursor - pool->begin) / pool->stride;
    if (count == 0)
    {
        engine_heap_free(pool); // 0x000663E0
        return false;
    }
    const float fraction = next_random_fraction(random);
    const i32 chosen = _mm_cvtt_ss2si(_mm_set_ss(fraction * float(count)));
    i32 index;
    std::memcpy(&index, pool->begin + chosen * 0xC, 4);
    const NavCell& selected = g_nav_grids[floor]->cells[index];
    output = {float(selected.x), float(selected.bottom), float(selected.z)};
    engine_heap_free(pool); // 0x000663E0
    const float half = half_navigation_cell();
    const float low_jitter = 0.f - half;
    output.x = (half - low_jitter) * next_random_fraction(random) + low_jitter + output.x;
    output.z = (half - low_jitter) * next_random_fraction(random) + low_jitter + output.z;
    return settle_navigation_point(floor, output); // 0x00111B40
}
}
