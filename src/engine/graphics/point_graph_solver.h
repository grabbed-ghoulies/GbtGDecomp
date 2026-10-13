#pragma once
#include "point_graph.h"
#include "gbtg/entity.h"

namespace gbtg
{
// Point rows (stride 0x30): position +0, previous position +0xC, acceleration +0x18, inverse mass +0x24,
// pinned byte +0x28. Edge rows (stride 0x1C): point indices +0/+4, point addresses +8/+0xC, rest length
// +0x10, stiffness +0x14, squared rest length +0x18. Descriptor: +0 points, +8 edges, +0x10 free edges,
// +0x18 sorted, +0x19 squared mode, +0x1C damping.
void step_runtime_point_graph(RuntimePointGraph& graph, float step); // Xbox 0x00102500, ESI graph, XMM1 step
void relax_runtime_point_graph_edge(RuntimePointGraph& graph, u32 first, u32 second,
    float length); // Xbox 0x00101C80, ECX first, EDX second
void relax_runtime_point_graph_edge_squared(RuntimePointGraph& graph, u32 first, u32 second,
    float stiffness); // Xbox 0x00101DD0, ECX first, EDX second
void hold_runtime_point_graph_edge(RuntimePointGraph& graph, u32 anchor, u32 moved,
    float length); // Xbox 0x00101F10, EAX anchor, ESI moved, EDX graph
void relax_runtime_point_graph(RuntimePointGraph& graph); // Xbox 0x00102030, EAX graph
void relax_sorted_runtime_point_graph(RuntimePointGraph& graph); // Xbox 0x00102280, EDI graph
// Records (stride 0x28) after a count word: position +0, radius +0x20. The nearest record whose radius
// holds the point, or none.
const u8* find_runtime_sphere_record(const Vec3& point, std::span<const u8> records); // Xbox 0x00052040, EDX point
// Stores the sixteen values column by column (out[0], out[4], out[8], out[12], out[1], ...).
void set_runtime_matrix_columns(float* output, const std::array<float, 16>& values); // Xbox 0x00153A00
// A collision triangle the graph step keeps (retail stride 0x28): vertex pointers +0/+4/+8, unit normal +0xC,
// its copy +0x18 and plane offset +0x24.
struct RuntimeGraphContact
{
    const Vec3* first = nullptr;
    const Vec3* second = nullptr;
    const Vec3* third = nullptr;
    Vec3 normal{};
    Vec3 plane{};
    float offset = 0.f;
};
// The segment start->finish, then finish->start, against each contact triangle (0x00103400).
u32 cross_runtime_graph_contacts(const Vec3& start, const Vec3& finish, u8& flag, u8& reversed, Vec3& normal,
    Vec3& point, std::span<const RuntimeGraphContact> contacts, i32 count,
    const RuntimeGraphContact*& hit); // Xbox 0x0010C100, EAX flag, EBX start
// Each flagged edge that crosses a contact moves its far point onto the surface and damps its previous position.
const RuntimeGraphContact* project_runtime_point_graph_edges(RuntimePointGraph& graph,
    std::span<const RuntimeGraphContact> contacts, i32 count); // Xbox 0x0010C340, ESI graph
// Grid: vertices +8, triangles +0xC (stride 0xC), cells x/y/z +0x18/+0x1C/+0x20, cell size +0x28,
// origin +0x2C, cell rows +0x44 (count, triangle index list).
void step_runtime_point_graph_collision(RuntimePointGraph& graph, float step, std::span<const u8> grid,
    const std::function<std::span<const u8>(u32, size_t)>& resolve); // Xbox 0x0010CFE0, XMM1 step
// Rest +0x24 coordinates, transforms +0x70 (rest), +0xB0 (current) and +0x170 (inverse), reference edges
// +0x1BC/+0x1C0/+0x1C4 and the re-base counter +0x1C8.
void recover_runtime_point_graph_transform(RuntimePointGraph& graph); // Xbox 0x0010C8D0, EBX graph
// The rotation taking from onto to; parallel inputs keep the identity and return 0 when opposite.
u32 runtime_rotation_between(float* matrix, const Vec3& from, const Vec3& to); // Xbox 0x0010BEB0, ESI matrix, EAX from
}
