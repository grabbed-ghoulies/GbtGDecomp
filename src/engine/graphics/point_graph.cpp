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
        PointGraphPoint& point = graph.point(index);
        std::memcpy(&point.position, samples->begin + index * 12, 12);
        point.force.x = 0.f;
        point.force.z = 0.f;
        point.force.y = -2.5f;
        point.inverse_mass = 1.f;
        point.pinned = 0;
    }
    engine_heap_free(samples);
    for (u32 index = 0; index < points; ++index)
    {
        graph.point(index).previous = graph.point(index).position;
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
                const Vec3& left = graph.point(first).position;
                const Vec3& right = graph.point(second).position;
                const float dz = left.z - right.z;
                const float dy = left.y - right.y;
                const float dx = left.x - right.x;
                const float squared = (dz * dz + dy * dy) + dx * dx;
                if (squared > 0.f)
                {
                    if (std::bit_cast<i32>(candidate) % stride == 0 ||
                        (first != 0 && first < 4) || (second != 0 && second < 4))
                    {
                        const ExtendedFloat ex = ExtendedFloat{left.x} -
                            ExtendedFloat{right.x};
                        const ExtendedFloat ey = ExtendedFloat{left.y} -
                            ExtendedFloat{right.y};
                        const ExtendedFloat ez = ExtendedFloat{left.z} -
                            ExtendedFloat{right.z};
                        const float distance = float(extended_sqrt((ez * ez + ey * ey) + ex * ex));
                        if (graph.header().sorted == 0)
                        {
                            const u32 edges = graph.header().edges;
                            PointGraphEdge& edge = graph.edge(edges);
                            edge.first = first;
                            edge.second = second;
                            edge.length = distance;
                            const float distance_squared = distance * distance;
                            edge.inverse_squared_length = 1.f / distance_squared;
                            edge.squared_length = distance_squared;
                            graph.header().edges = edges + 1;
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
        graph.coordinate(index) = graph.point(index).position;
    }
    const u32 edges = graph.header().edges;
    if (edges != 0)
    {
        step.x = step.x * 2.f;
        for (u32 index = 0; index < edges; ++index)
        {
            const u32 first = graph.edge(index).first;
            const u32 second = graph.edge(index).second;
            float dx = graph.point(first).position.x - graph.point(second).position.x;
            float dy = graph.point(first).position.y - graph.point(second).position.y;
            float dz = graph.point(first).position.z - graph.point(second).position.z;
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
                graph.record().reference_edges[0] = index;
            }
            if ((first == 0 && second == 2) || (first == 2 && second == 0))
            {
                graph.record().reference_edges[1] = index;
            }
            if ((first == 0 && second == 3) || (first == 3 && second == 0))
            {
                graph.record().reference_edges[2] = index;
            }
        }
    }
    for (u32 index = 0; index < edges; ++index)
    {
        if (graph.edge_flags[index] == 0)
        {
            const u32 first = graph.edge(index).first;
            const u32 second = graph.edge(index).second;
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
    PointGraphRecord& record = graph.record();
    record.coordinates_address = address + coordinate_offset;
    record.edge_flags_address = address + flag_offset;
    record.age = 0.f;
    record.edges_address_copy = address;
    record.points_address_copy = address + point_offset;
    record.header.sorted = 0;
    record.header.free_edges = 0;
    record.header.points_address = address + point_offset;
    record.header.points = points;
    record.header.edges_address = address;
    record.header.edges = 0;
    record.header.edge_capacity = edges;
    record.header.squared = 1;
    record.iterations = 5;
    record.warm_up_frames = 0;
    record.active = 0;
    record.contacted = 0;
    record.header.damping = 0.01f; // DAT_0032B124.
}

// Xbox 0x001021A0
i32 compare_runtime_point_graph_edges(const void* first, const void* second)
{
    const auto& left = *static_cast<const PointGraphEdge*>(first);
    const auto& right = *static_cast<const PointGraphEdge*>(second);
    const RuntimePointGraph& graph = *g_point_graph_sort;
    if ((graph.point(left.second).pinned | graph.point(left.first).pinned) != 0)
    {
        return (graph.point(right.first).pinned | graph.point(right.second).pinned) == 0;
    }
    return -1;
}

// Xbox 0x00102200
void sort_runtime_point_graph_edges(RuntimePointGraph& graph)
{
    g_point_graph_sort = &graph;
    runtime_quick_sort(graph.edges.data(), graph.header().edges, sizeof(PointGraphEdge), compare_runtime_point_graph_edges);
    const u32 count = graph.header().edges;
    g_point_graph_sort = nullptr;
    graph.header().free_edges = count;
    for (u32 index = 0; index < count; ++index)
    {
        const u32 first = graph.edge(index).first;
        const u32 second = graph.edge(index).second;
        if (graph.point(first).pinned != 0 || graph.point(second).pinned != 0)
        {
            graph.header().free_edges = index;
            return;
        }
    }
}

// Xbox 0x001024A0
void prepare_runtime_point_graph_edges(RuntimePointGraph& graph)
{
    if (graph.header().sorted == 0)
    {
        graph.header().sorted = 1;
        sort_runtime_point_graph_edges(graph);
        const u32 count = graph.header().edges;
        for (u32 index = 0; index < count; ++index)
        {
            PointGraphEdge& edge = graph.edge(index);
            edge.first_address = graph.points_address + edge.first * u32(sizeof(PointGraphPoint));
            edge.second_address = graph.points_address + edge.second * u32(sizeof(PointGraphPoint));
        }
    }
}
}
