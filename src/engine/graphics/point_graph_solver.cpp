#include "point_graph_solver.h"
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
constexpr size_t kPoint = 0x30;
constexpr size_t kEdge = 0x1C;

float* point_floats(RuntimePointGraph& graph, u32 index)
{
    if ((size_t(index) + 1) * kPoint > graph.points.size())
    {
        throw Error("point graph index is outside its point rows");
    }
    return reinterpret_cast<float*>(graph.points.data() + size_t(index) * kPoint);
}

float point_real(RuntimePointGraph& graph, u32 index, size_t offset)
{
    float value;
    std::memcpy(&value, graph.points.data() + size_t(index) * kPoint + offset, 4);
    return value;
}

void point_store(RuntimePointGraph& graph, u32 index, size_t offset, float value)
{
    std::memcpy(graph.points.data() + size_t(index) * kPoint + offset, &value, 4);
}

bool pinned(const RuntimePointGraph& graph, u32 index)
{
    return graph.points[size_t(index) * kPoint + 0x28] != 0;
}

u32 edge_word(const RuntimePointGraph& graph, u32 edge, size_t offset)
{
    return read_u32(graph.edges, size_t(edge) * kEdge + offset);
}

float edge_real(const RuntimePointGraph& graph, u32 edge, size_t offset)
{
    return read_f32(graph.edges, size_t(edge) * kEdge + offset);
}

// Edge +8 / +0xC hold point addresses that 0x001024A0 derives from the canonical points address.
u32 edge_point(const RuntimePointGraph& graph, u32 edge, size_t offset)
{
    const u32 address = edge_word(graph, edge, offset);
    const u32 delta = address - graph.points_address;
    if (delta % kPoint != 0 || delta / kPoint >= graph.points.size() / kPoint)
    {
        throw Error("point graph edge address does not name a point row");
    }
    return delta / kPoint;
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
    const u32 first = edge_word(graph, edge, 0);
    const u32 second = edge_word(graph, edge, 4);
    if (pinned(graph, first))
    {
        hold_runtime_point_graph_edge(graph, first, second, edge_real(graph, edge, 0x10));
        return true;
    }
    if (pinned(graph, second))
    {
        hold_runtime_point_graph_edge(graph, second, first, edge_real(graph, edge, 0x10));
        return true;
    }
    return false;
}
}

// Xbox 0x00102500
void step_runtime_point_graph(RuntimePointGraph& graph, float step)
{
    const float keep = 1.f - read_f32(graph.fields, 0x1C); // DAT_0032B0F8.
    const float squared = step * step;
    for (u32 index = 0; index < read_u32(graph.fields, 0); ++index)
    {
        if (pinned(graph, index))
        {
            continue;
        }
        const float x = point_real(graph, index, 0);
        const float y = point_real(graph, index, 4);
        const float z = point_real(graph, index, 8);
        point_store(graph, index, 0, (x - point_real(graph, index, 0xC)) * keep
            + point_real(graph, index, 0x18) * squared + x);
        point_store(graph, index, 4, (y - point_real(graph, index, 0x10)) * keep
            + point_real(graph, index, 0x1C) * squared + y);
        point_store(graph, index, 8, (z - point_real(graph, index, 0x14)) * keep
            + point_real(graph, index, 0x20) * squared + z);
        point_store(graph, index, 0xC, x);
        point_store(graph, index, 0x10, y);
        point_store(graph, index, 0x14, z);
    }
}

// Xbox 0x00101C80
void relax_runtime_point_graph_edge(RuntimePointGraph& graph, u32 first, u32 second, float length)
{
    float* a = point_floats(graph, first);
    float* b = point_floats(graph, second);
    const float dx = b[0] - a[0];
    const float dy = b[1] - a[1];
    const float dz = b[2] - a[2];
    const float distance = float(std::sqrt(double(dx) * dx + double(dz) * dz + double(dy) * dy));
    constexpr float kMinimum = std::bit_cast<float>(0x38D1B717u); // DAT_0032B200.
    if (!(distance > kMinimum))
    {
        return;
    }
    const float scale = (distance - length) / ((b[9] + a[9]) * distance);
    const float move_first = scale * a[9];
    const float move_second = 0.f - b[9] * scale;
    a[0] = a[0] + dx * move_first;
    a[1] = a[1] + move_first * dy;
    a[2] = dz * move_first + a[2];
    b[0] = b[0] + dx * move_second;
    b[1] = dy * move_second + b[1];
    b[2] = b[2] + dz * move_second;
}

// Xbox 0x00101DD0
void relax_runtime_point_graph_edge_squared(RuntimePointGraph& graph, u32 first, u32 second, float stiffness)
{
    float* a = point_floats(graph, first);
    float* b = point_floats(graph, second);
    const float dz = b[2] - a[2];
    const float dx = b[0] - a[0];
    const float dy = b[1] - a[1];
    const float stretch = (dx * dx + dz * dz + dy * dy) * stiffness;
    const float scale = (stretch - 1.f) / ((b[9] + a[9]) * (stretch + 1.f));
    const float move_first = a[9] * scale;
    const float move_second = 0.f - b[9] * scale;
    a[0] = a[0] + dx * move_first;
    a[1] = a[1] + move_first * dy;
    a[2] = dz * move_first + a[2];
    b[0] = b[0] + dx * move_second;
    b[1] = dy * move_second + b[1];
    b[2] = b[2] + dz * move_second;
}

// Xbox 0x00101F10
void hold_runtime_point_graph_edge(RuntimePointGraph& graph, u32 anchor, u32 moved, float length)
{
    const float* a = point_floats(graph, anchor);
    float* b = point_floats(graph, moved);
    const float ax = a[0];
    const float ay = a[1];
    const float az = a[2];
    const float dx = b[0] - ax;
    const float dy = b[1] - ay;
    const float dz = b[2] - az;
    float scale;
    if (graph.fields[0x19] != 0)
    {
        scale = 2.f / ((dx * dx + dz * dz + dy * dy) * (1.f / (length * length)) + 1.f); // DAT_0032B110.
    }
    else
    {
        scale = float(length / std::sqrt(double(dx) * dx + double(dz) * dz + double(dy) * dy));
    }
    b[0] = dx * scale + ax;
    b[1] = dy * scale + ay;
    b[2] = dz * scale + az;
}

// Xbox 0x00102030
void relax_runtime_point_graph(RuntimePointGraph& graph)
{
    if (graph.fields[0x18] != 0)
    {
        relax_sorted_runtime_point_graph(graph);
        return;
    }
    const bool squared = graph.fields[0x19] != 0;
    for (u32 edge = 0; edge < read_u32(graph.fields, 8); ++edge)
    {
        if (hold_pinned_edge(graph, edge))
        {
            continue;
        }
        const u32 first = edge_word(graph, edge, 0);
        const u32 second = edge_word(graph, edge, 4);
        if (squared)
        {
            relax_runtime_point_graph_edge_squared(graph, first, second, edge_real(graph, edge, 0x14));
        }
        else
        {
            relax_runtime_point_graph_edge(graph, first, second, edge_real(graph, edge, 0x10));
        }
    }
}

// Xbox 0x00102280
void relax_sorted_runtime_point_graph(RuntimePointGraph& graph)
{
    if (graph.fields[0x19] == 0)
    {
        for (u32 edge = 0; edge < read_u32(graph.fields, 8); ++edge)
        {
            if (!hold_pinned_edge(graph, edge))
            {
                relax_runtime_point_graph_edge(graph, edge_word(graph, edge, 0), edge_word(graph, edge, 4),
                    edge_real(graph, edge, 0x10));
            }
        }
        return;
    }
    for (u32 edge = 0; edge < read_u32(graph.fields, 0x10); ++edge)
    {
        float* a = point_floats(graph, edge_point(graph, edge, 8));
        float* b = point_floats(graph, edge_point(graph, edge, 0xC));
        const float dx = b[0] - a[0];
        const float dz = b[2] - a[2];
        const float dy = b[1] - a[1];
        const float rest = edge_real(graph, edge, 0x18);
        const float squared = dx * dx + dz * dz + dy * dy;
        const float scale = (squared - rest) / ((rest + squared) * 2.f); // DAT_0032B110.
        const float mx = dx * scale;
        const float my = dy * scale;
        const float mz = dz * scale;
        a[0] = a[0] + mx;
        a[1] = a[1] + my;
        a[2] = mz + a[2];
        b[0] = b[0] - mx;
        b[1] = b[1] - my;
        b[2] = b[2] - mz;
    }
    for (u32 edge = read_u32(graph.fields, 0x10); edge < read_u32(graph.fields, 8); ++edge)
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
    for (u32 edge = 0; edge < read_u32(graph.fields, 8); ++edge)
    {
        if (graph.edge_flags[edge] == 0)
        {
            continue;
        }
        const u32 first = edge_word(graph, edge, 0);
        const u32 second = edge_word(graph, edge, 4);
        const float* a = point_floats(graph, first);
        const float* b = point_floats(graph, second);
        const Vec3 start{a[0], a[1], a[2]};
        const Vec3 finish{b[0], b[1], b[2]};
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
        float* p = point_floats(graph, moved);
        const float t = ((point.z - p[2]) * normal.z + (point.y - p[1]) * normal.y + (point.x - p[0]) * normal.x)
            / (normal.z * normal.z + normal.y * normal.y + normal.x * normal.x);
        const Vec3 on{base.x + normal.x * t, base.y + normal.y * t, base.z + normal.z * t};
        const float x = normal.x * kLift + on.x;
        const float y = normal.y * kLift + on.y;
        const float z = normal.z * kLift + on.z;
        const float dx = x - p[3];
        const float dy = y - p[4];
        const float dz = z - p[5];
        const float along = dz * normal.z + dy * normal.y + dx * normal.x;
        const float tangent_x = dx - along * normal.x;
        const float tangent_y = dy - along * normal.y;
        const float tangent_z = dz - along * normal.z;
        const float gz = p[2] - z;
        const float gy = p[1] - y;
        const float gx = p[0] - x;
        float friction = kFriction / (gz * gz + gy * gy + gx * gx);
        if (friction > kFrictionLimit)
        {
            friction = kFrictionLimit;
        }
        p[3] = x - (tangent_x * friction + along * normal.x * kBounce);
        p[4] = y - (tangent_y * friction + along * normal.y * kBounce);
        p[5] = z - (tangent_z * friction + along * normal.z * kBounce);
        p[0] = x;
        p[1] = y;
        p[2] = z;
    }
    return hit;
}

// Xbox 0x0010CFE0
void step_runtime_point_graph_collision(RuntimePointGraph& graph, float step, std::span<const u8> grid,
    const std::function<std::span<const u8>(u32, size_t)>& resolve)
{
    float farthest = 0.f;
    step_runtime_point_graph(graph, step);
    const u32 count = read_u32(graph.fields, 0);
    float sum_x = 0.f;
    float sum_y = 0.f;
    float sum_z = 0.f;
    // Retail walks every second point (0x60 per pass) for both the centre and the radius.
    const u32 passes = count == 0 ? 0 : ((count - 1) >> 1) + 1;
    for (u32 pass = 0; pass < passes; ++pass)
    {
        sum_x = point_real(graph, pass * 2, 0) + sum_x;
        sum_y = point_real(graph, pass * 2, 4) + sum_y;
        sum_z = point_real(graph, pass * 2, 8) + sum_z;
    }
    const long double scale = 2.0L / (long double)(count); // DAT_0032B110 over the unsigned FILD.
    const Vec3 center{float(sum_x * scale), float(sum_y * scale), float(sum_z * scale)};
    for (u32 pass = 0; pass < passes; ++pass)
    {
        const float dy = center.y - point_real(graph, pass * 2, 4);
        const float dx = center.x - point_real(graph, pass * 2, 0);
        const float dz = center.z - point_real(graph, pass * 2, 8);
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
    for (u32 iteration = 0; iteration < graph.fields[0x1D4]; ++iteration)
    {
        relax_runtime_point_graph(graph);
        const RuntimeGraphContact* hit = project_runtime_point_graph_edges(graph, contacts, found);
        if (hit != nullptr)
        {
            graph.fields[0x1D6] = 1;
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
    const auto edge_point_index = [&graph](u32 field, size_t end)
    {
        return edge_word(graph, read_u32(graph.fields, field), end);
    };
    const auto rest = [&graph](u32 index)
    {
        return Vec3{read_f32(graph.coordinates, size_t(index) * 0xC), read_f32(graph.coordinates, size_t(index) * 0xC + 4),
            read_f32(graph.coordinates, size_t(index) * 0xC + 8)};
    };
    const auto current = [&graph](u32 index)
    {
        return Vec3{point_real(graph, index, 0), point_real(graph, index, 4), point_real(graph, index, 8)};
    };
    const auto difference = [](const Vec3& to, const Vec3& from)
    {
        return Vec3{to.x - from.x, to.y - from.y, to.z - from.z};
    };
    const u32 first_a = edge_point_index(0x1BC, 0);
    const u32 first_b = edge_point_index(0x1BC, 4);
    const u32 second_a = edge_point_index(0x1C0, 0);
    const u32 second_b = edge_point_index(0x1C0, 4);
    const u32 third_a = edge_point_index(0x1C4, 0);
    const u32 third_b = edge_point_index(0x1C4, 4);
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
    std::array<float, 16> rest_matrix;
    std::memcpy(rest_matrix.data(), graph.fields.data() + 0x70, 0x40);
    multiply_runtime_graphics_matrices(shift.data(), rest_matrix.data(), shift.data());
    multiply_runtime_graphics_matrices(turn.data(), shift.data(), turn.data());
    runtime_graphics_translation_matrix(shift.data(), offset.x + origin.x, origin.y + offset.y, origin.z + offset.z);
    std::array<float, 16> placed;
    multiply_runtime_graphics_matrices(placed.data(), turn.data(), shift.data());
    std::memcpy(graph.fields.data() + 0xB0, placed.data(), 0x40);
    std::array<float, 16> inverse;
    invert_runtime_graphics_matrix(inverse, nullptr, placed);
    std::memcpy(graph.fields.data() + 0x170, inverse.data(), 0x40);
    const u32 frames = read_u32(graph.fields, 0x1C8) + 1;
    std::memcpy(graph.fields.data() + 0x1C8, &frames, 4);
    if (frames > 10)
    {
        for (u32 index = 0; index < read_u32(graph.fields, 0); ++index)
        {
            std::memcpy(graph.coordinates.data() + size_t(index) * 0xC, graph.points.data() + size_t(index) * kPoint,
                0xC);
        }
        std::memcpy(graph.fields.data() + 0x70, graph.fields.data() + 0xB0, 0x40);
        const u32 zero = 0;
        std::memcpy(graph.fields.data() + 0x1C8, &zero, 4);
    }
}
}
