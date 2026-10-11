#include "path_routes.h"
#include "../entities/entity_heap.h"
#include "../entities/ghouly_box_decoder.h"
#include "gbtg/anim.h"
#include <cstring>
#include <new>
#include <bit>
#include <cmath>

namespace gbtg::entity_detail
{
namespace
{
void store_point(u8* row, i32 cell, const Vec3& point)
{
    std::memcpy(row, &cell, 4);
    std::memcpy(row + 4, &point.x, 4);
    std::memcpy(row + 8, &point.y, 4);
    std::memcpy(row + 0xC, &point.z, 4);
}
u8* append_row(GhoulySpawnPool*& route)
{
    if (route->cursor == route->limit)
    {
        route = grow_ghouly_spawn_pool(route);
    }
    u8* row = route->cursor;
    route->cursor += route->stride;
    return row;
}
}

// Xbox 0x000377B0
long double navigation_route_length(const GhoulySpawnPool& route)
{
    const auto accumulate = [](const u8* first, const u8* second, float sum)
    {
        float a[3];
        float b[3];
        std::memcpy(a, first + 4, 12);
        std::memcpy(b, second + 4, 12);
        const float dx = a[0] - b[0];
        const float dy = a[1] - b[1];
        const float dz = a[2] - b[2];
        return ((dz * dz + dx * dx) + dy * dy) + sum;
    };
    float sum = 0.0f;
    const u8* previous = route.begin;
    const u8* next = previous + 0x10;
    const i32 remaining_bytes = i32(route.cursor - next) + 15;
    const i32 rounded = (remaining_bytes + (remaining_bytes < 0 ? 15 : 0)) >> 4;
    if (rounded >= 4)
    {
        u32 blocks = (u32(i32(route.cursor - next) - 0x31) >> 6) + 1;
        next += blocks * 0x40;
        do
        {
            sum = accumulate(previous, previous + 0x10, sum);
            sum = accumulate(previous + 0x10, previous + 0x20, sum);
            sum = accumulate(previous + 0x20, previous + 0x30, sum);
            sum = accumulate(previous + 0x30, previous + 0x40, sum);
            previous += 0x40;
            --blocks;
        }
        while (blocks != 0);
    }
    if (next < route.cursor)
    {
        do
        {
            sum = accumulate(previous, next, sum);
            previous = next;
            next += 0x10;
        }
        while (next < route.cursor);
    }
    return retail_square_root(sum);
}

// Xbox 0x00037F50
float navigation_route_curve_parameter(const Vec3* points, float distance)
{
    float lengths[3];
    for (i32 index = 0; index < 3; ++index)
    {
        const long double dx = static_cast<long double>(points[index].x) - points[index + 1].x;
        const long double dy = static_cast<long double>(points[index].y) - points[index + 1].y;
        const long double dz = static_cast<long double>(points[index].z) - points[index + 1].z;
        // The dump uses inline x87 FSQRT, not the scalar square-root callee.
        lengths[index] = float(std::sqrt((dz * dz + dx * dx) + dy * dy));
    }
    const float total = (lengths[2] + lengths[1]) + lengths[0];
    const float cumulative[4] = {0.0f, lengths[0], lengths[0] + lengths[1],
        (lengths[0] + lengths[1]) + lengths[2]};
    if (distance > total)
    {
        distance = total;
    }
    i32 index = 0;
    while (!(distance >= cumulative[index]) || !(cumulative[index + 1] >= distance))
    {
        ++index;
        if (index >= 3)
        {
            return 1.0f;
        }
    }
    const float fraction = lengths[index] != 0.0f ?
        (distance - cumulative[index]) / lengths[index] : 1.0f;
    const float third = std::bit_cast<float>(u32(0x3EAAAAAB)); // DAT_0032B3D4.
    return float(index) * third + fraction * third;
}

// Xbox 0x00037D50
void navigation_route_direction(const GhoulySpawnPool& route, Vec3& direction,
    Vec3* sampled, float distance)
{
    const auto point = [](const u8* row)
    {
        Vec3 result;
        std::memcpy(&result.x, row + 4, 4);
        std::memcpy(&result.y, row + 8, 4);
        std::memcpy(&result.z, row + 0xC, 4);
        return result;
    };
    const Vec3 first = point(route.begin);
    if (distance == 0.0f)
    {
        const Vec3 second = point(route.begin + route.stride);
        direction = Vec3{second.x - first.x, second.y - first.y, second.z - first.z};
        if (sampled != nullptr)
        {
            *sampled = first;
        }
        return;
    }
    Vec3 controls[4];
    const u8* row = route.begin;
    if (u32((route.cursor - route.begin) / route.stride) > 1)
    {
        row += 0x10;
    }
    i32 index = 0;
    do
    {
        if (row >= route.cursor)
        {
            if (index < 4)
            {
                const Vec3 last = point(route.begin +
                    ((route.cursor - route.begin) / route.stride - 1) * route.stride);
                do
                {
                    controls[index] = last;
                    ++index;
                }
                while (index < 4);
            }
            break;
        }
        controls[index] = point(row);
        ++index;
        row += 0x10;
    }
    while (index < 4);
    const float parameter = navigation_route_curve_parameter(controls, distance);
    const float inverse = 1.0f - parameter;
    const float inverse_squared = inverse * inverse;
    const float second_weight = (parameter * 3.0f) * inverse_squared;
    const float parameter_squared = parameter * parameter;
    const float third_weight = (parameter_squared * 3.0f) * inverse;
    const float first_weight = inverse_squared * inverse;
    const float fourth_weight = parameter_squared * parameter;
    Vec3 result;
    float* const components = &result.x;
    for (i32 component = 0; component < 3; ++component)
    {
        components[component] = ((reinterpret_cast<const float*>(&controls[3])[component] * fourth_weight +
            reinterpret_cast<const float*>(&controls[2])[component] * third_weight) +
            reinterpret_cast<const float*>(&controls[0])[component] * first_weight) +
            reinterpret_cast<const float*>(&controls[1])[component] * second_weight;
    }
    direction = Vec3{result.x - first.x, result.y - first.y, result.z - first.z};
    *sampled = result;
}

// Xbox 0x00066310
u8* insert_navigation_route_row(GhoulySpawnPool*& route, i32 index)
{
    const i32 count = i32((route->cursor - route->begin) / route->stride);
    u8* row = append_row(route);
    if (index != count)
    {
        row = route->begin + index * route->stride;
        u8* const destination = route->begin + (index + 1) * route->stride;
        std::memmove(destination, row, route->cursor - destination);
    }
    return row;
}

// Xbox 0x00037980
u32 retarget_navigation_search_route(NavigationPathSearch& search, i32 start, i32 finish)
{
    auto* route = static_cast<GhoulySpawnPool*>(search.scratch);
    if (route == nullptr)
    {
        return 0;
    }
    if (i32(search.word_30) >= 10)
    {
        return 0;
    }
    const NavGrid& grid = *search.grid;
    const auto point = [&grid](i32 index)
    {
        const NavCell& cell = grid.cells[index];
        return Vec3{float(cell.x), float(cell.bottom), float(cell.z)};
    };
    if (navigation_neighbor_connected(grid, u32(start), u32(finish)))
    {
        route->cursor = route->begin;
        append_navigation_route_point(route, start, point(start));
        search.scratch = route;
        append_navigation_route_point(route, finish, point(finish));
        search.scratch = route;
        search.last = start;
        search.first = finish;
        search.word_30 = 0;
        return 1;
    }
    const i32 old_finish = search.first;
    bool changed = false;
    if (search.last != start)
    {
        const i32 count = i32((route->cursor - route->begin) / route->stride);
        i32 connected = -1;
        if (count < 1)
        {
            return 0;
        }
        for (i32 index = 0; index < count; ++index)
        {
            u32 cell;
            std::memcpy(&cell, route->begin + index * route->stride, 4);
            if (navigation_neighbor_connected(grid, cell, u32(start)))
            {
                connected = index;
            }
        }
        if (connected == -1)
        {
            return 0;
        }
        while (connected > 0)
        {
            if (route->begin < route->cursor - route->stride)
            {
                std::memmove(route->begin, route->begin + route->stride,
                    route->cursor - route->stride - route->begin);
            }
            route->cursor -= route->stride;
            --connected;
        }
        store_point(insert_navigation_route_row(route, 0), start, point(start));
        search.scratch = route;
        search.last = start;
        changed = true;
    }
    if (old_finish == finish)
    {
        if (!changed)
        {
            return 1;
        }
    }
    else
    {
        const i32 count = i32((route->cursor - route->begin) / route->stride);
        i32 connected = -1;
        if (count - 1 < 0)
        {
            return 0;
        }
        for (i32 index = count - 1; index >= 0; --index)
        {
            u32 cell;
            std::memcpy(&cell, route->begin + index * route->stride, 4);
            if (navigation_neighbor_connected(grid, cell, u32(finish)))
            {
                connected = index;
            }
        }
        if (connected == -1)
        {
            return 0;
        }
        i32 remove = count - connected - 1;
        while (remove > 0)
        {
            u8* const row = route->begin + (connected + 1) * route->stride;
            if (row < route->cursor - route->stride)
            {
                std::memmove(row, row + route->stride, route->cursor - route->stride - row);
            }
            route->cursor -= route->stride;
            --remove;
        }
        const i32 remaining = i32((route->cursor - route->begin) / route->stride);
        if (remaining > 0)
        {
            u8* const row = route->begin + (remaining - 1) * route->stride;
            i32 cell;
            std::memcpy(&cell, row, 4);
            const Vec3 coordinates = point(cell);
            std::memcpy(row + 4, &coordinates.x, 4);
            std::memcpy(row + 0xC, &coordinates.z, 4);
        }
        append_navigation_route_point(route, finish, point(finish));
        search.scratch = route;
        search.first = finish;
    }
    if ((route->cursor - route->begin) / route->stride > 0)
    {
        ++search.word_30;
    }
    return 1;
}

// Xbox 0x000376B0
void append_navigation_route_point(GhoulySpawnPool*& route, i32 cell, const Vec3& point)
{
    store_point(append_row(route), cell, point);
}

// Xbox 0x000376F0
void set_navigation_route_endpoints(GhoulySpawnPool& route, const Vec3& start, const Vec3& finish)
{
    u8* row = route.begin;
    std::memcpy(row + 4, &start.x, 4);
    std::memcpy(row + 0xC, &start.z, 4);
    const i32 count = i32(route.cursor - route.begin) / i32(route.stride);
    row = route.begin + (count - 1) * route.stride;
    std::memcpy(row + 4, &finish.x, 4);
    std::memcpy(row + 0xC, &finish.z, 4);
}

// Xbox 0x00037720
u32 navigation_route_is_open(const GhoulySpawnPool& route, const NavigationPathSearch& search)
{
    const u8* row = route.begin;
    const NavCell& first = search.grid->cells[i32(read_u32({row, 4}, 0))];
    if ((first.flags_7 & 2) != 0 || ((first.region & 0xFFF) != 0 && (first.region & 0x1000) != 0))
    {
        return 0;
    }
    do
    {
        row += 0x10;
        if (row >= route.cursor - 0x10)
        {
            return 1;
        }
        const NavCell& cell = search.grid->cells[i32(read_u32({row, 4}, 0))];
        const u16 occupancy = u16(cell.value_6) | (u16(cell.flags_7) << 8);
        if ((occupancy & 0x200) != 0)
        {
            return 0;
        }
        const u16 region = cell.region;
        if ((region & 0xFFF) != 0 && (region & 0x1000) != 0)
        {
            return 0;
        }
        if (!((region & 0x8000) != 0 || (occupancy & 0x1F) == 0 || (region & 0xFFF) != 0))
        {
            return 0;
        }
    }
    while (true);
}

// Xbox 0x00036E90
u32 navigation_path_search_route_is_open(const NavigationPathSearch& search)
{
    if (search.scratch != nullptr)
    {
        if (navigation_route_is_open(*static_cast<const GhoulySpawnPool*>(search.scratch), search) != 0)
        {
            return 1;
        }
    }
    return 0;
}

// Xbox 0x00037470
void build_navigation_search_route(const NavigationPathSearch& search,
    GhoulySpawnPool*& route, const Vec3& start, const Vec3& finish)
{
    if (route == nullptr)
    {
        route = static_cast<GhoulySpawnPool*>(engine_heap_allocate(sizeof(GhoulySpawnPool) + 0x40));
        auto* storage = reinterpret_cast<u8*>(route + 1);
        new (route) GhoulySpawnPool{0x10, 4, storage, storage, storage + 0x40};
    }
    else
    {
        route->cursor = route->begin;
    }
    const u8* node = search.nodes + search.last * 0x14;
    const u8* first = search.nodes + search.first * 0x14;
    i32 cell_index = i16(read_u16({node, 0x14}, 0xA));
    const NavCell& cell = search.grid->cells[cell_index];
    u8* row = append_row(route);
    store_point(row, cell_index, Vec3{float(cell.x), float(cell.bottom), float(cell.z)});
    std::memcpy(row + 4, &start.x, 4);
    std::memcpy(row + 0xC, &start.z, 4);
    const i16 previous = i16(read_u16({node, 0x14}, 8));
    if (previous != -1)
    {
        node = search.nodes + i32(previous) * 0x14;
        while (node != first)
        {
            cell_index = i16(read_u16({node, 0x14}, 0xA));
            const NavCell& intermediate = search.grid->cells[cell_index];
            row = append_row(route);
            store_point(row, cell_index, Vec3{float(intermediate.x), float(intermediate.bottom), float(intermediate.z)});
            node = search.nodes + i32(i16(read_u16({node, 0x14}, 8))) * 0x14;
        }
    }
    cell_index = search.first;
    const NavCell& final = search.grid->cells[cell_index];
    row = append_row(route);
    store_point(row, cell_index, Vec3{float(final.x), float(final.bottom), float(final.z)});
    std::memcpy(row + 4, &finish.x, 4);
    std::memcpy(row + 0xC, &finish.z, 4);
}

// Xbox 0x00036E50
GhoulySpawnPool* take_navigation_search_route(NavigationPathSearch& search,
    const Vec3& start, const Vec3& finish)
{
    if (search.scratch != nullptr)
    {
        engine_heap_free(search.scratch);
        search.scratch = nullptr;
    }
    search.word_30 = 0;
    GhoulySpawnPool* route = nullptr;
    build_navigation_search_route(search, route, start, finish);
    search.scratch = route;
    return route;
}
}
