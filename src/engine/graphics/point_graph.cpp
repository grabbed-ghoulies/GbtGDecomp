#include "point_graph.h"
#include "../entities/entity_heap.h"
#include "../runtime/crt_sort.h"
#include "../navigation/cell_registration.h"
#include "extended_float.h"

#include <bit>
#include <cstring>

namespace gbtg
{
RuntimePointGraph* g_point_graph_sort = nullptr;

namespace
{
void word(std::span<u8> fields, size_t offset, u32 value)
{
    std::memcpy(fields.data() + offset, &value, 4);
}
}

// Xbox 0x0010B1F0
void build_runtime_point_graph(RuntimePointGraph& graph, std::span<const Vec3> source, u32 count,
    const std::function<u32(u8*, u32)>& bind_allocation)
{
    using entity_detail::NavigationCellRegistration;
    const Vec3 seed = source[0];
    const Vec3 seed_x{seed.x + 10.f, seed.y, seed.z};
    const Vec3 seed_y{seed.x, seed.y + 10.f, seed.z};
    const Vec3 seed_z{seed.x, seed.y, seed.z + 10.f};
    auto* samples = static_cast<NavigationCellRegistration*>(
        engine_heap_allocate(i32(sizeof(NavigationCellRegistration)) + 24));
    samples->stride = 12;
    samples->growth = 2;
    samples->begin = reinterpret_cast<u8*>(samples + 1);
    samples->end = samples->begin;
    samples->capacity = samples->begin + 24;
    const auto append = [&](const Vec3& point)
    {
        if (samples->end == samples->capacity)
        {
            samples = entity_detail::grow_navigation_cell_registration(samples);
        }
        u8* row = samples->end;
        samples->end += samples->stride;
        std::memcpy(row, &point, 12);
    };
    append(seed);
    append(seed_x);
    append(seed_y);
    append(seed_z);
    Vec3 low = source[1];
    Vec3 high = source[1];
    for (u32 index = 2; index < count; ++index)
    {
        const Vec3& point = source[index];
        if (low.x > point.x)
        {
            low.x = point.x;
        }
        if (low.y > point.y)
        {
            low.y = point.y;
        }
        if (low.z > point.z)
        {
            low.z = point.z;
        }
        if (point.x > high.x)
        {
            high.x = point.x;
        }
        if (point.y > high.y)
        {
            high.y = point.y;
        }
        if (point.z > high.z)
        {
            high.z = point.z;
        }
    }
    const Vec3 extent{high.x - low.x, high.y - low.y, high.z - low.z};
    Vec3 step{extent.x * .25f, extent.y * .25f, extent.z * .25f};
    if (.5f > step.x)
    {
        step.x = .5f;
    }
    if (.5f > step.y)
    {
        step.y = .5f;
    }
    if (.5f > step.z)
    {
        step.z = .5f;
    }
    if (step.x >= extent.x)
    {
        step.x = extent.x * .51f;
    }
    if (step.y >= extent.y)
    {
        step.y = extent.y * .51f;
    }
    if (step.z >= extent.z)
    {
        step.z = extent.z * .51f;
    }
    for (float z = low.z; high.z > z; z = z + step.z)
    {
        for (float y = low.y; high.y > y; y = y + step.y)
        {
            for (float x = low.x; high.x > x; x = x + step.x)
            {
                Vec3 total{0.f, 0.f, 0.f};
                u32 included = 0;
                if (!(seed.x >= x && x + step.x > seed.x &&
                    seed.y >= y && y + step.y > seed.y &&
                    seed.z >= z && z + step.z > seed.z) && count > 1)
                {
                    for (u32 index = 1; index < count; ++index)
                    {
                        const Vec3& point = source[index];
                        if (point.x >= x && x + step.x >= point.x &&
                            point.y >= y && y + step.y >= point.y &&
                            point.z >= z && z + step.z >= point.z)
                        {
                            total.x = point.x + total.x;
                            total.y = total.y + point.y;
                            total.z = total.z + point.z;
                            ++included;
                        }
                    }
                    if (included != 0)
                    {
                        const ExtendedFloat reciprocal = ExtendedFloat{1.} / ExtendedFloat{double(included)};
                        append({float(ExtendedFloat{total.x} * reciprocal),
                            float(ExtendedFloat{total.y} * reciprocal), float(ExtendedFloat{total.z} * reciprocal)});
                    }
                }
            }
        }
    }
    const u32 points = u32(samples->end - samples->begin) / samples->stride;
    const u32 capacity = ((points + 1u) * points) >> 1;
    allocate_runtime_point_graph_storage(graph, points, capacity, bind_allocation);
    for (u32 index = 0; index < points; ++index)
    {
        const size_t row = index * 0x30u;
        std::memcpy(graph.points.data() + row, samples->begin + index * 12, 12);
        word(graph.points, row + 0x18, 0);
        word(graph.points, row + 0x20, 0);
        word(graph.points, row + 0x1C, std::bit_cast<u32>(-2.5f));
        word(graph.points, row + 0x24, std::bit_cast<u32>(1.f));
        graph.points[row + 0x28] = 0;
    }
    engine_heap_free(samples);
    for (u32 index = 0; index < points; ++index)
    {
        std::memcpy(graph.points.data() + index * 0x30u + 12,
            graph.points.data() + index * 0x30u, 12);
    }
    auto* valencies = static_cast<u8*>(engine_heap_allocate(i32(points)));
    for (u32 index = 0; index < points; ++index)
    {
        valencies[index] = 0;
    }
    const i32 stride = points <= 16 ? 1 : points <= 32 ? 2 : points > 40 ? 6 : 3;
    u32 candidate = 0;
    if (points != 1)
    {
        for (u32 first = 0; first < points - 1; ++first)
        {
            for (u32 second = first + 1; second < points; ++second)
            {
                const size_t left = first * 0x30u;
                const size_t right = second * 0x30u;
                const float dz = read_f32(graph.points, left + 8) - read_f32(graph.points, right + 8);
                const float dy = read_f32(graph.points, left + 4) - read_f32(graph.points, right + 4);
                const float dx = read_f32(graph.points, left) - read_f32(graph.points, right);
                const float squared = (dz * dz + dy * dy) + dx * dx;
                if (squared > 0.f)
                {
                    if (std::bit_cast<i32>(candidate) % stride == 0 ||
                        (first != 0 && first < 4) || (second != 0 && second < 4))
                    {
                        const ExtendedFloat ex = ExtendedFloat{read_f32(graph.points, left)} -
                            ExtendedFloat{read_f32(graph.points, right)};
                        const ExtendedFloat ey = ExtendedFloat{read_f32(graph.points, left + 4)} -
                            ExtendedFloat{read_f32(graph.points, right + 4)};
                        const ExtendedFloat ez = ExtendedFloat{read_f32(graph.points, left + 8)} -
                            ExtendedFloat{read_f32(graph.points, right + 8)};
                        const float distance = float(extended_sqrt((ez * ez + ey * ey) + ex * ex));
                        if (graph.fields[0x18] == 0)
                        {
                            const u32 edges = read_u32(graph.fields, 8);
                            const size_t edge = edges * 0x1Cu;
                            word(graph.edges, edge, first);
                            word(graph.edges, edge + 4, second);
                            word(graph.edges, edge + 0x10, std::bit_cast<u32>(distance));
                            const float distance_squared = distance * distance;
                            word(graph.edges, edge + 0x14, std::bit_cast<u32>(1.f / distance_squared));
                            word(graph.edges, edge + 0x18, std::bit_cast<u32>(distance_squared));
                            word(graph.fields, 8, edges + 1);
                        }
                    }
                    ++candidate;
                }
            }
        }
    }
    prepare_runtime_point_graph_edges(graph);
    for (u32 index = 0; index < points; ++index)
    {
        std::memcpy(graph.coordinates.data() + index * 12, graph.points.data() + index * 0x30u, 12);
    }
    const u32 edges = read_u32(graph.fields, 8);
    if (edges != 0)
    {
        step.x = step.x * 2.f;
        for (u32 index = 0; index < edges; ++index)
        {
            const u32 first = read_u32(graph.edges, index * 0x1Cu);
            const u32 second = read_u32(graph.edges, index * 0x1Cu + 4);
            float dx = read_f32(graph.points, first * 0x30u) - read_f32(graph.points, second * 0x30u);
            float dy = read_f32(graph.points, first * 0x30u + 4) - read_f32(graph.points, second * 0x30u + 4);
            float dz = read_f32(graph.points, first * 0x30u + 8) - read_f32(graph.points, second * 0x30u + 8);
            if (0.f > dx)
            {
                dx = 0.f - dx;
            }
            if (0.f > dy)
            {
                dy = 0.f - dy;
            }
            if (0.f > dz)
            {
                dz = 0.f - dz;
            }
            if (!(step.x > dx && step.y * 2.f > dy && step.z * 2.f > dz) ||
                (first != 0 && first < 4) || (second != 0 && second < 4))
            {
                graph.edge_flags[index] = 0;
            }
            else if ((valencies[first] < 4 && valencies[second] < 6) ||
                (valencies[second] < 4 && valencies[first] < 6))
            {
                graph.edge_flags[index] = 1;
                ++valencies[first];
                ++valencies[second];
            }
            else
            {
                graph.edge_flags[index] = 0;
            }
            if ((first == 0 && second == 1) || (first == 1 && second == 0))
            {
                word(graph.fields, 0x1BC, index);
            }
            if ((first == 0 && second == 2) || (first == 2 && second == 0))
            {
                word(graph.fields, 0x1C0, index);
            }
            if ((first == 0 && second == 3) || (first == 3 && second == 0))
            {
                word(graph.fields, 0x1C4, index);
            }
        }
    }
    for (u32 index = 0; index < edges; ++index)
    {
        if (graph.edge_flags[index] == 0)
        {
            const u32 first = read_u32(graph.edges, index * 0x1Cu);
            const u32 second = read_u32(graph.edges, index * 0x1Cu + 4);
            if ((first == 0 || first > 3) && (second == 0 || second > 3) &&
                (valencies[first] < 2 || valencies[second] < 2))
            {
                graph.edge_flags[index] = 1;
                ++valencies[first];
                ++valencies[second];
            }
        }
    }
    engine_heap_free(valencies);
}

// Xbox 0x0010B160
void allocate_runtime_point_graph_storage(RuntimePointGraph& graph, u32 points, u32 edges,
    const std::function<u32(u8*, u32)>& bind_allocation)
{
    const u32 bytes = points * 0x3Cu + edges * 0x1Du;
    graph.allocation = static_cast<u8*>(engine_heap_allocate(std::bit_cast<i32>(bytes)));
    const u32 address = bind_allocation(graph.allocation, bytes);
    const u32 point_offset = edges * 0x1Cu;
    const u32 coordinate_offset = point_offset + points * 0x30u;
    const u32 flag_offset = coordinate_offset + points * 0xCu;
    graph.points = {graph.allocation + point_offset, points * 0x30u};
    graph.edges = {graph.allocation, edges * 0x1Cu};
    graph.coordinates = {graph.allocation + coordinate_offset, points * 0xCu};
    graph.edge_flags = {graph.allocation + flag_offset, edges};
    graph.points_address = address + point_offset;
    word(graph.fields, 0x24, address + coordinate_offset);
    word(graph.fields, 0x2C, address + flag_offset);
    word(graph.fields, 0x1D0, 0);
    word(graph.fields, 0x28, address);
    word(graph.fields, 0x20, address + point_offset);
    graph.fields[0x18] = 0;
    word(graph.fields, 0x10, 0);
    word(graph.fields, 4, address + point_offset);
    word(graph.fields, 0, points);
    word(graph.fields, 0x14, address);
    word(graph.fields, 8, 0);
    word(graph.fields, 0xC, edges);
    graph.fields[0x19] = 1;
    graph.fields[0x1D4] = 5;
    word(graph.fields, 0x1C8, 0);
    graph.fields[0x1D5] = 0;
    graph.fields[0x1D6] = 0;
    word(graph.fields, 0x1C, 0x3C23D70Au); // DAT_0032B124, 0.01f.
}

// Xbox 0x001021A0
i32 compare_runtime_point_graph_edges(const void* first, const void* second)
{
    const std::span<const u8> left{static_cast<const u8*>(first), 0x1C};
    const std::span<const u8> right{static_cast<const u8*>(second), 0x1C};
    const auto points = g_point_graph_sort->points;
    if ((points[read_u32(left, 4) * 0x30u + 0x28] |
        points[read_u32(left, 0) * 0x30u + 0x28]) != 0)
    {
        return (points[read_u32(right, 0) * 0x30u + 0x28] |
            points[read_u32(right, 4) * 0x30u + 0x28]) == 0;
    }
    return -1;
}

// Xbox 0x00102200
void sort_runtime_point_graph_edges(RuntimePointGraph& graph)
{
    g_point_graph_sort = &graph;
    runtime_quick_sort(graph.edges.data(), read_u32(graph.fields, 8), 0x1C, compare_runtime_point_graph_edges);
    const u32 count = read_u32(graph.fields, 8);
    g_point_graph_sort = nullptr;
    word(graph.fields, 0x10, count);
    for (u32 index = 0; index < count; ++index)
    {
        const u32 first = read_u32(graph.edges, index * 0x1C);
        const u32 second = read_u32(graph.edges, index * 0x1C + 4);
        if (graph.points[first * 0x30u + 0x28] != 0 || graph.points[second * 0x30u + 0x28] != 0)
        {
            word(graph.fields, 0x10, index);
            return;
        }
    }
}

// Xbox 0x001024A0
void prepare_runtime_point_graph_edges(RuntimePointGraph& graph)
{
    if (graph.fields[0x18] == 0)
    {
        graph.fields[0x18] = 1;
        sort_runtime_point_graph_edges(graph);
        const u32 count = read_u32(graph.fields, 8);
        for (u32 index = 0; index < count; ++index)
        {
            const u32 first = read_u32(graph.edges, index * 0x1C);
            const u32 second = read_u32(graph.edges, index * 0x1C + 4);
            word(graph.edges, index * 0x1C + 8, graph.points_address + first * 0x30u);
            word(graph.edges, index * 0x1C + 0xC, graph.points_address + second * 0x30u);
        }
    }
}
}
