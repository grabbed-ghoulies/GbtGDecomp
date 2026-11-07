#pragma once

#include "gbtg/retail_field.h"
#include "gbtg/types.h"
#include <array>
#include <functional>
#include <span>

namespace gbtg
{
// One point (0x30 bytes): 0x00102500 moves it by its motion since the last step and its force.
struct PointGraphPoint
{
    Vec3 position{}; // +0
    Vec3 previous{}; // +0xC
    Vec3 force{}; // +0x18, multiplied by the step squared
    float inverse_mass = 0.f; // +0x24, an edge's relaxation moves each end by its share (0x00101C80)
    u8 pinned = 0; // +0x28, stays where it is and holds its edges' other ends
    std::array<u8, 7> unk_29{}; // +0x29
};
static_assert(sizeof(PointGraphPoint) == 0x30);

// One edge (0x1C bytes), between two points by index and by address.
struct PointGraphEdge
{
    u32 first = 0; // +0
    u32 second = 0; // +4
    u32 first_address = 0; // +8, from 0x001024A0
    u32 second_address = 0; // +0xC
    float length = 0.f; // +0x10, at rest
    float inverse_squared_length = 0.f; // +0x14
    float squared_length = 0.f; // +0x18
};
static_assert(sizeof(PointGraphEdge) == 0x1C);

// What the solver reads, the start of both a graph's record and a cloth's header (BackgroundClothHeader).
struct PointGraphHeader
{
    u32 points = 0; // +0
    u32 points_address = 0; // +4
    u32 edges = 0; // +8
    u32 edge_capacity = 0; // +0xC
    u32 free_edges = 0; // +0x10, those sorted before the edges with a pinned end (0x00102200)
    u32 edges_address = 0; // +0x14
    u8 sorted = 0; // +0x18
    u8 squared = 0; // +0x19, relax by squared lengths
    std::array<u8, 2> unk_1A{}; // +0x1A
    float damping = 0.f; // +0x1C
};
static_assert(sizeof(PointGraphHeader) == 0x20);

// A background animation's graph (0x1E0 bytes, a row at actor +0x950).
struct PointGraphRecord
{
    PointGraphHeader header{}; // +0
    u32 points_address_copy = 0; // +0x20
    u32 coordinates_address = 0; // +0x24
    u32 edges_address_copy = 0; // +0x28
    u32 edge_flags_address = 0; // +0x2C
    std::array<float, 16> bind_matrix{}; // +0x30, the model's matrix
    std::array<float, 16> rest_matrix{}; // +0x70, the pose the coordinates were taken in
    std::array<float, 16> placed_matrix{}; // +0xB0, recovered from the points each step
    std::array<u8, 0x40> unk_F0{}; // +0xF0
    std::array<float, 16> bind_inverse{}; // +0x130
    std::array<float, 16> placed_inverse{}; // +0x170
    Vec3 center{}; // +0x1B0, the model points' average
    std::array<u32, 3> reference_edges{}; // +0x1BC, the edges from point 0 to points 1, 2 and 3
    u32 warm_up_frames = 0; // +0x1C8, past 10 the current pose becomes the rest pose
    std::array<u8, 4> unk_1CC{}; // +0x1CC
    float age = 0.f; // +0x1D0, seconds since it was set active
    u8 iterations = 0; // +0x1D4, relaxations per collision step
    u8 active = 0; // +0x1D5
    u8 contacted = 0; // +0x1D6, a relaxation met the collision grid
    std::array<u8, 9> unk_1D7{}; // +0x1D7
};
static_assert(sizeof(PointGraphRecord) == 0x1E0);

struct RuntimePointGraph
{
    std::span<u8> fields; // Descriptor, stride 0x1E0 in background animation rows.
    u8* allocation;
    std::span<u8> points; // +4 / +0x20; point rows have stride 0x30.
    std::span<u8> edges; // +0x14 / +0x28; edge rows have stride 0x1C.
    std::span<u8> coordinates; // +0x24; stride 0xC.
    std::span<u8> edge_flags; // +0x2C.
    u32 points_address; // Native owner's canonical address for points.

    // The cloth's descriptor is its own type, so only an animation's graph has a whole record to view.
    PointGraphHeader& header() const
    {
        detail::check_retail_range(fields.size(), 0, sizeof(PointGraphHeader));
        return *reinterpret_cast<PointGraphHeader*>(fields.data());
    }
    PointGraphRecord& record() const
    {
        detail::check_retail_range(fields.size(), 0, sizeof(PointGraphRecord));
        return *reinterpret_cast<PointGraphRecord*>(fields.data());
    }
    PointGraphPoint& point(u32 index) const
    {
        detail::check_retail_range(points.size(), size_t(index) * sizeof(PointGraphPoint), sizeof(PointGraphPoint));
        return *reinterpret_cast<PointGraphPoint*>(points.data() + size_t(index) * sizeof(PointGraphPoint));
    }
    PointGraphEdge& edge(u32 index) const
    {
        detail::check_retail_range(edges.size(), size_t(index) * sizeof(PointGraphEdge), sizeof(PointGraphEdge));
        return *reinterpret_cast<PointGraphEdge*>(edges.data() + size_t(index) * sizeof(PointGraphEdge));
    }
    Vec3& coordinate(u32 index) const
    {
        detail::check_retail_range(coordinates.size(), size_t(index) * sizeof(Vec3), sizeof(Vec3));
        return *reinterpret_cast<Vec3*>(coordinates.data() + size_t(index) * sizeof(Vec3));
    }
};
extern RuntimePointGraph* g_point_graph_sort; // DAT_005462E0.
void build_runtime_point_graph(RuntimePointGraph& graph, std::span<const Vec3> source, u32 count,
    const std::function<u32(u8*, u32)>& bind_allocation); // Xbox 0x0010B1F0
void allocate_runtime_point_graph_storage(RuntimePointGraph& graph, u32 points, u32 edges,
    const std::function<u32(u8*, u32)>& bind_allocation); // Xbox 0x0010B160
i32 compare_runtime_point_graph_edges(const void* first, const void* second); // Xbox 0x001021A0
void sort_runtime_point_graph_edges(RuntimePointGraph& graph); // Xbox 0x00102200
void prepare_runtime_point_graph_edges(RuntimePointGraph& graph); // Xbox 0x001024A0
}
