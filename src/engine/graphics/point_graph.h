#pragma once

#include "gbtg/types.h"
#include <functional>
#include <span>

namespace gbtg
{
struct RuntimePointGraph
{
    std::span<u8> fields; // Descriptor, stride 0x1E0 in background animation rows.
    u8* allocation;
    std::span<u8> points; // +4 / +0x20; point rows have stride 0x30.
    std::span<u8> edges; // +0x14 / +0x28; edge rows have stride 0x1C.
    std::span<u8> coordinates; // +0x24; stride 0xC.
    std::span<u8> edge_flags; // +0x2C.
    u32 points_address; // Native owner's canonical address for points.
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
