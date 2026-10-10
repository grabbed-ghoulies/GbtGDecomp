#include "cell_registration.h"
#include "../entities/entity_heap.h"
#include <bit>
#include <cstring>
#include <xmmintrin.h>

namespace gbtg::entity_detail
{
// Xbox 0x001108B0
void release_navigation_cell_registration(NavigationCellRegistration*& records, u32 grid)
{
    if (records != nullptr)
    {
        u8* end = records->end;
        NavGrid* owner = g_nav_grids[grid];
        for (u8* row = records->begin; row < end; row += 0xC)
        {
            u32 index;
            u32 selector;
            std::memcpy(&index, row, 4);
            std::memcpy(&selector, row + 4, 4);
            auto& cell = owner->cells[index];
            const u16 old = u16(cell.value_6) | (u16(cell.flags_7) << 8);
            u16 value;
            if (selector == 0)
            {
                value = u16((((u32(old) - 1) ^ old) & 0x1F) ^ old);
            }
            else
            {
                value = u16(((((u32(old >> 5) - 1) << 5) ^ old) & 0x1E0) ^ old);
            }
            cell.value_6 = u8(value);
            cell.flags_7 = u8(value >> 8);
        }
        engine_heap_free(records);
        records = nullptr;
    }
}

namespace
{
i32 truncate_float(float value)
{
    return _mm_cvttss_si32(_mm_set_ss(value));
}

u16 cell_counter_word(const NavCell& cell)
{
    return u16(u16(cell.value_6) | (u16(cell.flags_7) << 8));
}

void store_cell_counter_word(NavCell& cell, u16 value)
{
    cell.value_6 = u8(value);
    cell.flags_7 = u8(value >> 8);
}

// Shared row append of 0x001104E0: grow by the header's growth count when full.
u8* append_registration_row(NavigationCellRegistration*& records)
{
    NavigationCellRegistration* header = records;
    if (header->end == header->capacity)
    {
        const i32 stride = header->stride;
        const i32 count = i32(header->end - header->begin) / stride;
        const i32 rows = i32(header->growth) + count;
        auto* grown = static_cast<NavigationCellRegistration*>(
            engine_heap_reallocate(header, i32(sizeof(NavigationCellRegistration)) + stride * rows));
        u8* first = reinterpret_cast<u8*>(grown) + sizeof(NavigationCellRegistration);
        grown->begin = first;
        grown->end = first + grown->stride * count;
        grown->capacity = first + grown->stride * rows;
        records = grown;
        header = grown;
    }
    u8* row = header->end;
    header->end = row + header->stride;
    return row;
}
}

// Xbox 0x00110970
void clamped_navigation_cell_coordinates(const NavGrid& grid, const Vec3& point, i32& x, i32& z)
{
    i32 cell_x = std::bit_cast<i32>(u32(truncate_float(point.x)) - u32(g_nav_origin_x)) / grid.cell;
    i32 cell_z = std::bit_cast<i32>(u32(truncate_float(point.z)) - u32(g_nav_origin_z)) / grid.cell;
    if (cell_x <= 0)
    {
        cell_x = 0;
    }
    if (cell_x >= grid.width - 1)
    {
        cell_x = grid.width - 1;
    }
    if (cell_z <= 0)
    {
        cell_z = 0;
    }
    if (cell_z >= grid.depth - 1)
    {
        x = cell_x;
        z = grid.depth - 1;
        return;
    }
    x = cell_x;
    z = cell_z;
}

// Xbox 0x001109E0
u32 navigation_cell_bounds(i32 floor, const Vec3* corners, i32& min_x, i32& max_x, i32& min_z, i32& max_z)
{
    const NavGrid* grid = g_nav_grids[floor];
    if (grid == nullptr)
    {
        return 0;
    }
    i32 xs[4];
    i32 zs[4];
    for (size_t index = 0; index < 4; ++index)
    {
        clamped_navigation_cell_coordinates(*grid, corners[index], xs[index], zs[index]);
    }
    i32 low_x = xs[0] >= xs[1] ? xs[1] : xs[0];
    i32 high_x = xs[0] < xs[1] ? xs[1] : xs[0];
    i32 low_z = zs[0] >= zs[1] ? zs[1] : zs[0];
    i32 high_z = zs[0] < zs[1] ? zs[1] : zs[0];
    for (size_t index = 2; index < 4; ++index)
    {
        if (low_x >= xs[index])
        {
            low_x = xs[index];
        }
        if (high_x < xs[index])
        {
            high_x = xs[index];
        }
        if (low_z >= zs[index])
        {
            low_z = zs[index];
        }
        if (high_z < zs[index])
        {
            high_z = zs[index];
        }
    }
    min_x = low_x;
    max_x = high_x;
    min_z = low_z;
    max_z = high_z;
    return 1;
}

// Xbox 0x001104E0
void register_navigation_cells(NavigationCellRegistration*& records, const Vec3& center, i32 floor, float radius,
    i32 include_outer)
{
    NavGrid* grid = g_nav_grids[floor];
    if (grid == nullptr)
    {
        return;
    }
    const Vec3 corners[4]{{center.x - radius, center.y, center.z - radius}, {center.x + radius, center.y, center.z - radius},
        {center.x + radius, center.y, center.z + radius}, {center.x - radius, center.y, center.z + radius}};
    i32 min_x;
    i32 max_x;
    i32 min_z;
    i32 max_z;
    navigation_cell_bounds(floor, corners, min_x, max_x, min_z, max_z);
    if (records == nullptr)
    {
        auto* header = static_cast<NavigationCellRegistration*>(engine_heap_allocate(i32(sizeof(NavigationCellRegistration)) + 0x30));
        u8* first = reinterpret_cast<u8*>(header) + sizeof(NavigationCellRegistration);
        header->begin = first;
        header->end = first;
        header->stride = 0xC;
        header->growth = 4;
        header->capacity = first + 0x30;
        records = header;
    }
    else
    {
        u8* end = records->end;
        for (u8* row = records->begin; row < end; row += 0xC)
        {
            u32 index;
            u32 selector;
            std::memcpy(&index, row, 4);
            std::memcpy(&selector, row + 4, 4);
            auto& cell = grid->cells[index];
            const u16 old = cell_counter_word(cell);
            if (selector != 0)
            {
                store_cell_counter_word(cell, u16(((((u32(u16(old >> 5)) - 1) << 5) ^ old) & 0x1E0) ^ old));
            }
            else
            {
                store_cell_counter_word(cell, u16((((u32(old) - 1) ^ old) & 0x1F) ^ old));
            }
        }
        records->end = records->begin;
    }
    const float inner = radius * radius;
    const float half = g_nav_grids[0] != nullptr ? float(g_nav_grids[0]->cell) * 0.5f : 0.0f; // DAT_0032B0F4.
    const float reach = half + radius;
    const float outer = reach * reach;
    const i32 center_index = navigation_cell_index(floor, center);
    for (i32 x = min_x; x <= max_x; ++x)
    {
        for (i32 z = min_z; z <= max_z; ++z)
        {
            const i32 index = std::bit_cast<i32>(u32(grid->width) * u32(z) + u32(x));
            auto& cell = grid->cells[u32(index)];
            const float dx = float(cell.x) - center.x;
            const float dz = float(cell.z) - center.z;
            const float distance = dz * dz + dx * dx;
            if (distance > outer)
            {
                continue;
            }
            const u16 old = cell_counter_word(cell);
            u32 selector;
            if (include_outer != 0 || inner > distance || index == center_index)
            {
                store_cell_counter_word(cell, u16((((u32(old) + 1) ^ old) & 0x1F) ^ old));
                selector = 0;
            }
            else
            {
                store_cell_counter_word(cell, u16((((u32(old) & 0xFFE0) + 0x20 ^ old) & 0x1E0) ^ old));
                selector = 1;
            }
            u8* row = append_registration_row(records);
            std::memcpy(row + 4, &selector, 4);
            std::memcpy(row, &index, 4);
        }
    }
}
}
