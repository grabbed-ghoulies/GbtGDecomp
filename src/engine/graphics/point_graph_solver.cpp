#include "point_graph_solver.h"
#if !defined(GBTG_RETAIL_GAME_ONLY)
#include "../runtime/rare_replay.h"
#endif
#include "vector_normalize.h"
#include "d3dx_matrix.h"
#include "matrix_inverse.h"
#include "../entities/mesh_sphere_geometry.h"
#include "../entities/physical_mesh_queries.h"
#include <bit>
#include <cmath>
#include <cstring>

namespace gbtg
{
namespace
{
// An edge's point addresses, which 0x001024A0 derives from the canonical points address, as point indices.
u32 point_at_address(const RuntimePointGraph& graph, u32 address)
{
    const u32 delta = address - graph.points_address;
    if (delta % sizeof(PointGraphPoint) != 0 || delta / sizeof(PointGraphPoint) >= graph.points.size() / sizeof(PointGraphPoint))
    {
        throw Error("point graph edge address does not name a point row");
    }
    return delta / sizeof(PointGraphPoint);
}

// CVTTSS2SI: out-of-range and unordered inputs give the integer indefinite value.
i32 truncate_float(float value)
{
    if (!(value >= -2147483648.f && value < 2147483648.f))
    {
        return i32(0x80000000u);
    }
    return i32(value);
}

// The pinned-end pass shared by 0x00102030 and 0x00102280.
bool hold_pinned_edge(RuntimePointGraph& graph, u32 edge)
{
    const u32 first = graph.edge(edge).first;
    const u32 second = graph.edge(edge).second;
    if (graph.point(first).pinned != 0)
    {
        hold_runtime_point_graph_edge(graph, first, second, graph.edge(edge).length);
        return true;
    }
    if (graph.point(second).pinned != 0)
    {
        hold_runtime_point_graph_edge(graph, second, first, graph.edge(edge).length);
        return true;
    }
    return false;
}
}

// Xbox 0x00102500
void step_runtime_point_graph(RuntimePointGraph& graph, float step)
{
    const float keep = 1.f - graph.header().damping; // DAT_0032B0F8.
    const float squared = step * step;
    for (u32 index = 0; index < graph.header().points; ++index)
    {
        if (graph.point(index).pinned != 0)
        {
            continue;
        }
        const float x = graph.point(index).position.x;
        const float y = graph.point(index).position.y;
        const float z = graph.point(index).position.z;
        graph.point(index).position.x = (x - graph.point(index).previous.x) * keep
            + graph.point(index).force.x * squared + x;
        graph.point(index).position.y = (y - graph.point(index).previous.y) * keep
            + graph.point(index).force.y * squared + y;
        graph.point(index).position.z = (z - graph.point(index).previous.z) * keep
            + graph.point(index).force.z * squared + z;
        graph.point(index).previous.x = x;
        graph.point(index).previous.y = y;
        graph.point(index).previous.z = z;
    }
}

// Xbox 0x00101C80
void relax_runtime_point_graph_edge(RuntimePointGraph& graph, u32 first, u32 second, float length)
{
    PointGraphPoint& a = graph.point(first);
    PointGraphPoint& b = graph.point(second);
    const float dx = b.position.x - a.position.x;
    const float dy = b.position.y - a.position.y;
    const float dz = b.position.z - a.position.z;
    const float distance = float(std::sqrt(double(dx) * dx + double(dz) * dz + double(dy) * dy));
    constexpr float kMinimum = std::bit_cast<float>(0x38D1B717u); // DAT_0032B200.
    if (!(distance > kMinimum))
    {
        return;
    }
    const float scale = (distance - length) / ((b.inverse_mass + a.inverse_mass) * distance);
    const float move_first = scale * a.inverse_mass;
    const float move_second = 0.f - b.inverse_mass * scale;
    a.position.x = a.position.x + dx * move_first;
    a.position.y = a.position.y + move_first * dy;
    a.position.z = dz * move_first + a.position.z;
    b.position.x = b.position.x + dx * move_second;
    b.position.y = dy * move_second + b.position.y;
    b.position.z = b.position.z + dz * move_second;
}

// Xbox 0x00101DD0
void relax_runtime_point_graph_edge_squared(RuntimePointGraph& graph, u32 first, u32 second, float stiffness)
{
    PointGraphPoint& a = graph.point(first);
    PointGraphPoint& b = graph.point(second);
    const float dz = b.position.z - a.position.z;
    const float dx = b.position.x - a.position.x;
    const float dy = b.position.y - a.position.y;
    const float stretch = (dx * dx + dz * dz + dy * dy) * stiffness;
    const float scale = (stretch - 1.f) / ((b.inverse_mass + a.inverse_mass) * (stretch + 1.f));
    const float move_first = a.inverse_mass * scale;
    const float move_second = 0.f - b.inverse_mass * scale;
    a.position.x = a.position.x + dx * move_first;
    a.position.y = a.position.y + move_first * dy;
    a.position.z = dz * move_first + a.position.z;
    b.position.x = b.position.x + dx * move_second;
    b.position.y = dy * move_second + b.position.y;
    b.position.z = b.position.z + dz * move_second;
}

// Xbox 0x00101F10
void hold_runtime_point_graph_edge(RuntimePointGraph& graph, u32 anchor, u32 moved, float length)
{
    const PointGraphPoint& a = graph.point(anchor);
    PointGraphPoint& b = graph.point(moved);
    const float ax = a.position.x;
    const float ay = a.position.y;
    const float az = a.position.z;
    const float dx = b.position.x - ax;
    const float dy = b.position.y - ay;
    const float dz = b.position.z - az;
    float scale;
    if (graph.header().squared != 0)
    {
        scale = 2.f / ((dx * dx + dz * dz + dy * dy) * (1.f / (length * length)) + 1.f); // DAT_0032B110.
    }
    else
    {
        scale = float(length / std::sqrt(double(dx) * dx + double(dz) * dz + double(dy) * dy));
    }
    b.position.x = dx * scale + ax;
    b.position.y = dy * scale + ay;
    b.position.z = dz * scale + az;
}

// Xbox 0x00102030
void relax_runtime_point_graph(RuntimePointGraph& graph)
{
    if (graph.header().sorted != 0)
    {
        relax_sorted_runtime_point_graph(graph);
        return;
    }
    const bool squared = graph.header().squared != 0;
    for (u32 edge = 0; edge < graph.header().edges; ++edge)
    {
        if (hold_pinned_edge(graph, edge))
        {
            continue;
        }
        const u32 first = graph.edge(edge).first;
        const u32 second = graph.edge(edge).second;
        if (squared)
        {
            relax_runtime_point_graph_edge_squared(graph, first, second, graph.edge(edge).inverse_squared_length);
        }
        else
        {
            relax_runtime_point_graph_edge(graph, first, second, graph.edge(edge).length);
        }
    }
}

// Xbox 0x00102280
void relax_sorted_runtime_point_graph(RuntimePointGraph& graph)
{
    if (graph.header().squared == 0)
    {
        for (u32 edge = 0; edge < graph.header().edges; ++edge)
        {
            if (!hold_pinned_edge(graph, edge))
            {
                relax_runtime_point_graph_edge(graph, graph.edge(edge).first, graph.edge(edge).second,
                    graph.edge(edge).length);
            }
        }
        return;
    }
    for (u32 edge = 0; edge < graph.header().free_edges; ++edge)
    {
        PointGraphPoint& a = graph.point(point_at_address(graph, graph.edge(edge).first_address));
        PointGraphPoint& b = graph.point(point_at_address(graph, graph.edge(edge).second_address));
        const float dx = b.position.x - a.position.x;
        const float dz = b.position.z - a.position.z;
        const float dy = b.position.y - a.position.y;
        const float rest = graph.edge(edge).squared_length;
        const float squared = dx * dx + dz * dz + dy * dy;
        const float scale = (squared - rest) / ((rest + squared) * 2.f); // DAT_0032B110.
        const float mx = dx * scale;
        const float my = dy * scale;
        const float mz = dz * scale;
        a.position.x = a.position.x + mx;
        a.position.y = a.position.y + my;
        a.position.z = mz + a.position.z;
        b.position.x = b.position.x - mx;
        b.position.y = b.position.y - my;
        b.position.z = b.position.z - mz;
    }
    for (u32 edge = graph.header().free_edges; edge < graph.header().edges; ++edge)
    {
        hold_pinned_edge(graph, edge);
    }
}

// Xbox 0x00052040
const u8* find_runtime_sphere_record(const Vec3& point, std::span<const u8> records)
{
    const i32 count = i32(read_u32(records, 0));
    const u8* best = nullptr;
    float best_distance = 0.f;
    for (i32 index = 0; index < count; ++index)
    {
        const size_t offset = 4 + size_t(index) * 0x28;
        const float dz = point.z - read_f32(records, offset + 8);
        const float dx = point.x - read_f32(records, offset);
        const float dy = point.y - read_f32(records, offset + 4);
        const float distance = dx * dx + dz * dz + dy * dy;
        const float radius = read_f32(records, offset + 0x24);
        if ((best == nullptr || best_distance > distance) && radius * radius > distance)
        {
            best = records.data() + offset;
            best_distance = distance;
        }
    }
    return best;
}

// Xbox 0x00153A00
void set_runtime_matrix_columns(float* output, const std::array<float, 16>& values)
{
    for (size_t index = 0; index < 16; ++index)
    {
        output[(index % 4) * 4 + index / 4] = values[index];
    }
}

// Xbox 0x0010BEB0
u32 runtime_rotation_between(float* matrix, const Vec3& from, const Vec3& to)
{
    for (size_t index = 0; index < 16; ++index)
    {
        matrix[index] = index % 5 == 0 ? 1.f : 0.f;
    }
    Vec3 u{};
    normalize_runtime_graphics_vector(u, from);
    Vec3 v{};
    normalize_runtime_graphics_vector(v, to);
    const float cx = u.y * v.z - v.y * u.z;
    const float cy = u.z * v.x - u.x * v.z;
    const float cz = v.y * u.x - v.x * u.y;
    const float dot = u.x * v.x + v.y * u.y + u.z * v.z;
    const float squared = cz * cz + cy * cy + cx * cx;
    constexpr float kParallel = std::bit_cast<float>(0x3727C5ACu); // DAT_0032B230.
    if (!(squared > kParallel))
    {
        return 0.f > dot ? 0 : 1;
    }
    const float k = (1.f - dot) / squared;
    const float xy = cx * cy * k;
    const float xz = cx * cz * k;
    const float yz = cy * cz * k;
    set_runtime_matrix_columns(matrix, {cx * cx * k + dot, xy - cz, xz + cy, 0.f,
        xy + cz, cy * cy * k + dot, yz - cx, 0.f,
        xz - cy, yz + cx, cz * cz * k + dot, 0.f,
        0.f, 0.f, 0.f, 1.f});
    return 1;
}

// Xbox 0x0010C100
u32 cross_runtime_graph_contacts(const Vec3& start, const Vec3& finish, u8& flag, u8& reversed, Vec3& normal,
    Vec3& point, std::span<const RuntimeGraphContact> contacts, i32 count, const RuntimeGraphContact*& hit)
{
    flag = 0;
    hit = nullptr;
    for (i32 index = 0; index < count; ++index)
    {
        const Vec3 forward{finish.x - start.x, finish.y - start.y, finish.z - start.z};
        const Vec3 backward{start.x - finish.x, start.y - finish.y, start.z - finish.z};
        const RuntimeGraphContact& contact = contacts[size_t(index)];
        float fraction = 0.f;
        if (intersect_runtime_mesh_triangle(forward, *contact.second, *contact.third, *contact.first, start,
                fraction))
        {
            point = Vec3{forward.x * fraction + start.x, start.y + forward.y * fraction,
                start.z + forward.z * fraction};
            normal = contact.normal;
            reversed = 0;
            hit = &contact;
            return 1;
        }
        if (intersect_runtime_mesh_triangle(backward, *contact.second, *contact.third, *contact.first, finish,
                fraction))
        {
            point = Vec3{backward.x * fraction + finish.x, finish.y + backward.y * fraction,
                backward.z * fraction + finish.z};
            normal = contact.normal;
            reversed = 1;
            hit = &contact;
            return 1;
        }
    }
    return 0;
}

// Xbox 0x0010C340
const RuntimeGraphContact* project_runtime_point_graph_edges(RuntimePointGraph& graph,
    std::span<const RuntimeGraphContact> contacts, i32 count)
{
    const RuntimeGraphContact* hit = nullptr;
    constexpr float kLift = std::bit_cast<float>(0x3D4CCCCDu); // DAT_0032B250.
    constexpr float kFriction = std::bit_cast<float>(0x378637BEu); // DAT_0032B350.
    constexpr float kFrictionLimit = std::bit_cast<float>(0x3F666666u); // DAT_0032B174.
    constexpr float kBounce = std::bit_cast<float>(0x3F19999Au); // DAT_0032B248.
    for (u32 edge = 0; edge < graph.header().edges; ++edge)
    {
        if (graph.edge_flags[edge] == 0)
        {
            continue;
        }
        const u32 first = graph.edge(edge).first;
        const u32 second = graph.edge(edge).second;
        const PointGraphPoint& a = graph.point(first);
        const PointGraphPoint& b = graph.point(second);
        const Vec3 start{a.position.x, a.position.y, a.position.z};
        const Vec3 finish{b.position.x, b.position.y, b.position.z};
        u8 flag = 0;
        u8 reversed = 0;
        Vec3 normal{};
        Vec3 point{};
        if (cross_runtime_graph_contacts(start, finish, flag, reversed, normal, point, contacts, count, hit) == 0)
        {
            continue;
        }
        const u32 moved = reversed == 0 ? second : first;
        const Vec3& base = reversed == 0 ? finish : start;
        PointGraphPoint& p = graph.point(moved);
        const float t = ((point.z - p.position.z) * normal.z + (point.y - p.position.y) * normal.y + (point.x - p.position.x) * normal.x)
            / (normal.z * normal.z + normal.y * normal.y + normal.x * normal.x);
        const Vec3 on{base.x + normal.x * t, base.y + normal.y * t, base.z + normal.z * t};
        const float x = normal.x * kLift + on.x;
        const float y = normal.y * kLift + on.y;
        const float z = normal.z * kLift + on.z;
        const float dx = x - p.previous.x;
        const float dy = y - p.previous.y;
        const float dz = z - p.previous.z;
        const float along = dz * normal.z + dy * normal.y + dx * normal.x;
        const float tangent_x = dx - along * normal.x;
        const float tangent_y = dy - along * normal.y;
        const float tangent_z = dz - along * normal.z;
        const float gz = p.position.z - z;
        const float gy = p.position.y - y;
        const float gx = p.position.x - x;
        float friction = kFriction / (gz * gz + gy * gy + gx * gx);
        if (friction > kFrictionLimit)
        {
            friction = kFrictionLimit;
        }
        p.previous.x = x - (tangent_x * friction + along * normal.x * kBounce);
        p.previous.y = y - (tangent_y * friction + along * normal.y * kBounce);
        p.previous.z = z - (tangent_z * friction + along * normal.z * kBounce);
        p.position.x = x;
        p.position.y = y;
        p.position.z = z;
    }
    return hit;
}

// Xbox 0x0010CFE0
void step_runtime_point_graph_collision(RuntimePointGraph& graph, float step, std::span<const u8> grid,
    const std::function<std::span<const u8>(u32, size_t)>& resolve)
{
    float farthest = 0.f;
    step_runtime_point_graph(graph, step);
    const u32 count = graph.header().points;
    float sum_x = 0.f;
    float sum_y = 0.f;
    float sum_z = 0.f;
    // Retail walks every second point (0x60 per pass) for both the centre and the radius.
    const u32 passes = count == 0 ? 0 : ((count - 1) >> 1) + 1;
    for (u32 pass = 0; pass < passes; ++pass)
    {
        sum_x = graph.point(pass * 2).position.x + sum_x;
        sum_y = graph.point(pass * 2).position.y + sum_y;
        sum_z = graph.point(pass * 2).position.z + sum_z;
    }
    const long double scale = 2.0L / (long double)(count); // DAT_0032B110 over the unsigned FILD.
    const Vec3 center{float(sum_x * scale), float(sum_y * scale), float(sum_z * scale)};
    for (u32 pass = 0; pass < passes; ++pass)
    {
        const float dy = center.y - graph.point(pass * 2).position.y;
        const float dx = center.x - graph.point(pass * 2).position.x;
        const float dz = center.z - graph.point(pass * 2).position.z;
        const float distance = dz * dz + dy * dy + dx * dx;
        if (distance > farthest)
        {
            farthest = distance;
        }
    }
    const float inverse = 1.f / read_f32(grid, 0x28);
    i32 x = truncate_float((center.x - read_f32(grid, 0x2C)) * inverse);
    i32 y = truncate_float((center.y - read_f32(grid, 0x30)) * inverse);
    i32 z = truncate_float((center.z - read_f32(grid, 0x34)) * inverse);
    const float radius = float(std::sqrt(double(farthest)) * std::bit_cast<float>(0x3F99999Au)); // DAT_0032B24C.
    const auto clamp = [](i32 value, i32 limit)
    {
        if (value < 0)
        {
            return 0;
        }
        return value >= limit ? limit - 1 : value;
    };
    const i32 width = i32(read_u32(grid, 0x18));
    const i32 height = i32(read_u32(grid, 0x1C));
    x = clamp(x, width);
    y = clamp(y, height);
    z = clamp(z, i32(read_u32(grid, 0x20)));
    std::array<RuntimeGraphContact, 12> contacts{};
    i32 found = 0;
    const u32 cell = read_u32(grid, 0x44) + u32((height * z + y) * width + x) * 8;
    if (cell != 0)
    {
        const auto row = resolve(cell, 8);
        const u32 triangles = read_u32(row, 0);
        for (u32 entry = 0; entry < triangles; ++entry)
        {
            const u32 triangle = read_u32(resolve(read_u32(row, 4) + entry * 4, 4), 0);
            const auto corners = resolve(read_u32(grid, 0xC) + triangle * 0xC, 0xC);
            const auto vertex = [&](size_t corner)
            {
                return reinterpret_cast<const Vec3*>(
                    resolve(read_u32(grid, 8) + read_u32(corners, corner * 4) * 0xC, 0xC).data());
            };
            const Vec3* a = vertex(0);
            const Vec3* b = vertex(1);
            const Vec3* c = vertex(2);
            if (found >= 0xC)
            {
                break;
            }
            if (!runtime_sphere_intersects_triangle(center, radius, *a, *b, *c))
            {
                continue;
            }
            RuntimeGraphContact& contact = contacts[size_t(found)];
            contact.first = a;
            contact.second = b;
            contact.third = c;
            const float by = b->y - a->y;
            const float bz = b->z - a->z;
            const float bx = b->x - a->x;
            const float cy = c->y - a->y;
            const float cz = c->z - a->z;
            const float cx = c->x - a->x;
            const Vec3 cross{cz * by - cy * bz, bz * cx - cz * bx, cy * bx - by * cx};
            normalize_runtime_graphics_vector(contact.normal, cross);
            contact.plane = contact.normal;
            contact.offset = 0.f - (a->x * contact.normal.x + a->y * contact.normal.y + contact.normal.z * a->z);
            ++found;
        }
    }
    for (u32 iteration = 0; iteration < graph.record().iterations; ++iteration)
    {
        relax_runtime_point_graph(graph);
        const RuntimeGraphContact* hit = project_runtime_point_graph_edges(graph, contacts, found);
        if (hit != nullptr)
        {
            graph.record().contacted = 1;
            if (found > 1)
            {
                contacts[0] = *hit;
                found = 1;
            }
        }
    }
}

// Xbox 0x0010C8D0
void recover_runtime_point_graph_transform(RuntimePointGraph& graph)
{
    PointGraphRecord& record = graph.record();
    const auto rest = [&graph](u32 index)
    {
        return graph.coordinate(index);
    };
    const auto current = [&graph](u32 index)
    {
        return graph.point(index).position;
    };
    const auto difference = [](const Vec3& to, const Vec3& from)
    {
        return Vec3{to.x - from.x, to.y - from.y, to.z - from.z};
    };
    const u32 first_a = graph.edge(record.reference_edges[0]).first;
    const u32 first_b = graph.edge(record.reference_edges[0]).second;
    const u32 second_a = graph.edge(record.reference_edges[1]).first;
    const u32 second_b = graph.edge(record.reference_edges[1]).second;
    const u32 third_a = graph.edge(record.reference_edges[2]).first;
    const u32 third_b = graph.edge(record.reference_edges[2]).second;
    Vec3 rest_first = difference(rest(first_b), rest(first_a));
    Vec3 current_first = difference(current(first_b), current(first_a));
    Vec3 rest_second = difference(rest(second_b), rest(second_a));
    Vec3 current_second = difference(current(second_b), current(second_a));
    const double dot = (double(current_first.z) * rest_first.z + double(current_first.y) * rest_first.y)
        + double(current_first.x) * rest_first.x;
    const double current_length = std::sqrt((double(current_first.z) * current_first.z
        + double(current_first.y) * current_first.y) + double(current_first.x) * current_first.x);
    const double rest_length = std::sqrt((double(rest_first.z) * rest_first.z + double(rest_first.y) * rest_first.y)
        + double(rest_first.x) * rest_first.x);
    const double cosine = dot / (current_length * rest_length);
    u32 swapped = 0;
    constexpr float kLowCosine = std::bit_cast<float>(0xBF733333u); // DAT_0032B434.
    constexpr float kHighCosine = std::bit_cast<float>(0x3F733333u); // DAT_0032B2B8.
    if (kLowCosine > cosine || float(cosine) > kHighCosine)
    {
        rest_first = difference(rest(third_b), rest(third_a));
        current_first = difference(current(third_b), current(third_a));
        swapped = 1;
    }
    std::array<float, 16> turn{};
    if (runtime_rotation_between(turn.data(), rest_first, current_first) == 0)
    {
        return;
    }
    // Rotate the second rest edge and swing it about the first current edge onto the second current edge.
    const auto swing_matrix = [&](const Vec3& rest_edge, const Vec3& current_edge, const Vec3* projection,
                                  Vec3& kept_projection, std::array<float, 16>& swing)
    {
        Vec3 unit{};
        normalize_runtime_graphics_vector(unit, rest_edge);
        Vec3 turned{};
        transform_runtime_graphics_normal(turned, unit, turn.data());
        const float length = float(std::sqrt((double(rest_edge.z) * rest_edge.z + double(rest_edge.y) * rest_edge.y)
            + double(rest_edge.x) * rest_edge.x));
        if (projection == nullptr)
        {
            const float along = (current_edge.z * current_first.z + current_edge.y * current_first.y)
                + current_edge.x * current_first.x;
            const float span = (current_first.z * current_first.z + current_first.y * current_first.y)
                + current_first.x * current_first.x;
            const float scale = along / span;
            kept_projection = Vec3{current_first.x * scale, current_first.y * scale, current_first.z * scale};
            projection = &kept_projection;
        }
        const Vec3 scaled{turned.x * length, turned.y * length, turned.z * length};
        const Vec3 from = difference(scaled, *projection);
        const Vec3 to = difference(current_edge, *projection);
        return runtime_rotation_between(swing.data(), from, to);
    };
    std::array<float, 16> swing{};
    Vec3 projection{};
    if (swing_matrix(rest_second, current_second, nullptr, projection, swing) == 0)
    {
        if (swapped != 0)
        {
            return;
        }
        const Vec3 rest_third = difference(rest(third_b), rest(third_a));
        const Vec3 current_third = difference(current(third_b), current(third_a));
        Vec3 unused{};
        if (swing_matrix(rest_third, current_third, &projection, unused, swing) == 0)
        {
            return;
        }
    }
    multiply_runtime_graphics_matrices(turn.data(), turn.data(), swing.data());
    const Vec3 origin = rest(0);
    const Vec3 offset = difference(current(0), origin);
    std::array<float, 16> shift{};
    runtime_graphics_translation_matrix(shift.data(), 0.f - origin.x, 0.f - origin.y, 0.f - origin.z);
    const std::array<float, 16> rest_matrix = record.rest_matrix;
    multiply_runtime_graphics_matrices(shift.data(), rest_matrix.data(), shift.data());
    multiply_runtime_graphics_matrices(turn.data(), shift.data(), turn.data());
    runtime_graphics_translation_matrix(shift.data(), offset.x + origin.x, origin.y + offset.y, origin.z + offset.z);
    std::array<float, 16> placed;
    multiply_runtime_graphics_matrices(placed.data(), turn.data(), shift.data());
    record.placed_matrix = placed;
    std::array<float, 16> inverse;
    inverse = record.placed_inverse;
    invert_runtime_graphics_matrix(inverse, nullptr, placed);
    record.placed_inverse = inverse;
    u32 frames = record.warm_up_frames + 1;
#if !defined(GBTG_RETAIL_GAME_ONLY)
    // Rare Replay: the warm-up counts retail ticks.
    if (!rare_replay_retail_tick())
    {
        frames = record.warm_up_frames;
    }
#endif
    record.warm_up_frames = frames;
    if (frames > 10)
    {
        for (u32 index = 0; index < graph.header().points; ++index)
        {
            graph.coordinate(index) = graph.point(index).position;
        }
        record.rest_matrix = record.placed_matrix;
        record.warm_up_frames = 0;
    }
}
}
