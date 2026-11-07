#include "background_cloth.h"
#include "point_graph_solver.h"
#include "../camera/view_frustum.h"
#include "vector_normalize.h"
#include "../entities/actor_transform.h"
#include "../entities/entity_heap.h"
#include "../entities/entity_types.h"
#include "../cutscenes/door_registration.h"
#include "../runtime/crt_sort.h"
#include "gbtg/anim.h"

#include <bit>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace gbtg
{
namespace
{

Vec3 point_position(RuntimeBackgroundCloth& cloth, u32 index)
{
    const PointGraphPoint& point = cloth.point(index);
    return {point.position.x, point.position.y, point.position.z};
}

// x87 _ftol (0x001673D8): truncation toward zero.
i32 truncate_to_int(long double value)
{
    return i32(value);
}

u32 grey(i32 level)
{
    const u32 low = u32(level) & 0xFFu;
    return ((((u32(level) | 0xFFFFFF00u) << 8) | low) << 8) | low;
}

// Xbox 0x001151F0: the cloth falls asleep once every tenth point's last move averages under 1e-5 (x10).
void settle_when_still(RuntimeBackgroundCloth& cloth)
{
    const u32 count = cloth_header(cloth.bytes).points;
    float total = 0.f;
    if (count != 0)
    {
        for (u32 index = 0; index < (count - 1) / 10 + 1; ++index)
        {
            const PointGraphPoint& point = cloth.point(index * 10);
            total = (point.position.z - point.previous.z) * (point.position.z - point.previous.z) + (point.position.x - point.previous.x) * (point.position.x - point.previous.x)
                + (point.position.y - point.previous.y) * (point.position.y - point.previous.y) + total;
        }
    }
    float scale = float(i32(count));
    if (i32(count) < 0)
    {
        scale = scale + 4294967296.f; // DAT_0032B144
    }
    if (0.0001f <= total * 10.f / scale) // DAT_0032B200, DAT_0032B10C
    {
        return;
    }
    cloth_header(cloth.bytes).awake = u8(0);
}

// Xbox 0x001152C0: an actor within 40 of the bottom edge's middle wakes the cloth.
void wake_when_near(RuntimeBackgroundCloth& cloth, RuntimeBackgroundClothContext& context)
{
    const u32 rows = cloth_header(cloth.bytes).rows;
    const u32 columns = cloth_header(cloth.bytes).columns;
    const Vec3 corner = point_position(cloth, (columns - 1) * rows);
    const Vec3 last = point_position(cloth, rows * columns - 1);
    const Vec3 middle{(last.x + corner.x) * 0.5f, (last.y + corner.y) * 0.5f, (last.z + corner.z) * 0.5f};
    const Actor* found = nullptr;
    float distance = 0.f;
    if (!find_runtime_nearest_wide_actor(middle, found, distance, false, context)) // 0x00114490
    {
        return;
    }
    (void)actor_world_position(*found); // 0x000412A0; the position is not read.
    if (1600.f > distance) // DAT_0032B4CC
    {
        cloth_header(cloth.bytes).awake = u8(1);
    }
}
}

// Xbox 0x0004B5F0, AX columns
RuntimeBackgroundCloth* create_runtime_background_cloth(u16 rows, u16 columns, u16 edges,
    RuntimeBackgroundClothContext& context)
{
    const u32 count = u32(rows) * u32(columns);
    const u32 size = count * 0x40 + sizeof(BackgroundClothHeader) + u32(edges) * 0x1C;
    auto* block = static_cast<u8*>(engine_heap_allocate(i32(size)));
    auto* cloth = new RuntimeBackgroundCloth;
    cloth->bytes = {block, size};
    cloth->address = context.heap_address(block, size);
    const u32 points = cloth->address + sizeof(BackgroundClothHeader);
    const u32 edge_rows = points + count * 0x30;
    const u32 colors = edge_rows + u32(edges) * 0x1C;
    const u32 normals = colors + count * 4;
    auto bytes = cloth->bytes;
    std::memset(block, 0, sizeof(BackgroundClothHeader));
    cloth_header(bytes).rows = rows;
    cloth_header(bytes).points_address_copy = points;
    cloth_header(bytes).damping = 0.0001f; // DAT_0032B200
    cloth_header(bytes).points_address = points;
    cloth_header(bytes).columns = columns;
    cloth_header(bytes).light = Vec3{1.f, 0.f, 0.f}; // DAT_0032B0F8; +0x40 and +0x44 are cleared below.
    cloth_header(bytes).awake = u8(1);
    cloth_header(bytes).constrain_hook = 0u;
    cloth_header(bytes).constrain_enabled = 0u;
    cloth_header(bytes).awake_hook = 0u;
    cloth_header(bytes).asleep_hook = 0u;
    cloth_header(bytes).before_hook = 0u;
    cloth_header(bytes).unk_1e5 = u8(2);
    cloth_header(bytes).unk_1e4 = u8(0);
    cloth_header(bytes).texture = 0u;
    cloth_header(bytes).edges_address_copy = edge_rows;
    cloth_header(bytes).colors_address = colors;
    cloth_header(bytes).normals_address = normals;
    cloth_header(bytes).sorted = u8(0);
    cloth_header(bytes).free_edges = 0u;
    cloth_header(bytes).points = count;
    cloth_header(bytes).edges_address = edge_rows;
    cloth_header(bytes).edges = 0u;
    cloth_header(bytes).edge_capacity = u32(edges);
    cloth_header(bytes).squared = u8(1);
    cloth_header(bytes).light.y = 0.f;
    cloth_header(bytes).light.z = 0.f;
    cloth->planes.reserve(2); // 0x90 bytes: stride 0x40, growth 2.
    cloth_header(bytes).damping = 0.01f; // DAT_0032B124
    cloth_header(bytes).always_stepped = 1u;
    cloth_header(bytes).always_drawn = 0u;
    cloth_header(bytes).shadow_surface = 0u;
    cloth_header(bytes).shadow_texture = 0u;
    cloth_header(bytes).casts_shadow = u8(1);
    cloth_header(bytes).faces_room = u8(0);
    cloth_header(bytes).rustles = u8(0);
    cloth_header(bytes).sound_wait = 0.f;
    cloth_header(bytes).own_shaders = u8(1);
    cloth_header(bytes).second_draw = u8(0);
    cloth->graph.fields = bytes.subspan(0, sizeof(BackgroundClothHeader));
    cloth->graph.allocation = block;
    cloth->graph.points = bytes.subspan(sizeof(BackgroundClothHeader), count * 0x30);
    cloth->graph.edges = bytes.subspan(sizeof(BackgroundClothHeader) + count * 0x30, u32(edges) * 0x1C);
    cloth->graph.points_address = points;
    cloth->colors = bytes.subspan(sizeof(BackgroundClothHeader) + count * 0x30 + u32(edges) * 0x1C, count * 4);
    cloth->normals = bytes.subspan(sizeof(BackgroundClothHeader) + count * 0x34 + u32(edges) * 0x1C, count * 0xC);
    measure_runtime_background_cloth(*cloth); // 0x0004C0E0
    cloth_header(bytes).layer = u8(0);
    cloth_header(bytes).specular = u8(0);
    return cloth;
}

void release_runtime_background_cloth(RuntimeBackgroundCloth* cloth)
{
    engine_heap_free(cloth->bytes.data()); // 0x0014D360
    delete cloth;
}

// Xbox 0x0004C0E0, EAX cloth
void measure_runtime_background_cloth(RuntimeBackgroundCloth& cloth)
{
    Vec3 low = point_position(cloth, 0);
    Vec3 high = low;
    const u32 step = cloth_header(cloth.bytes).rows < 3 ? 1u : 3u;
    const u32 count = cloth_header(cloth.bytes).points;
    for (u32 index = 0; index < count; index += step)
    {
        const Vec3 point = point_position(cloth, index);
        if (point.x < high.x || point.x == high.x)
        {
            if (point.x <= low.x && low.x != point.x)
            {
                low.x = point.x;
            }
        }
        else
        {
            high.x = point.x;
        }
        if (point.y < high.y || point.y == high.y)
        {
            if (point.y <= low.y && low.y != point.y)
            {
                low.y = point.y;
            }
        }
        else
        {
            high.y = point.y;
        }
        if (point.z < high.z || point.z == high.z)
        {
            if (point.z <= low.z && low.z != point.z)
            {
                low.z = point.z;
            }
        }
        else
        {
            high.z = point.z;
        }
    }
    cloth_header(cloth.bytes).low = low;
    cloth_header(cloth.bytes).high = high;
}

// Xbox 0x0004C000, EAX cloth
void step_runtime_background_cloth(RuntimeBackgroundCloth& cloth, float step, RuntimeBackgroundClothContext& context)
{
    u32 hook;
    if (cloth_header(cloth.bytes).awake == 0)
    {
        hook = cloth_header(cloth.bytes).asleep_hook;
    }
    else
    {
        if (cloth_header(cloth.bytes).before_hook != 0)
        {
            call_runtime_background_cloth_hook(cloth_header(cloth.bytes).before_hook, cloth, context);
        }
        step_runtime_point_graph(cloth.graph, step); // 0x00102500, ESI cloth, XMM1 step
        const bool constrained = cloth_header(cloth.bytes).constrain_hook != 0
            && cloth_header(cloth.bytes).constrain_enabled != 0;
        for (u32 pass = 0; pass < 2; ++pass)
        {
            relax_runtime_point_graph(cloth.graph); // 0x00102030, EAX cloth
            if (constrained)
            {
                call_runtime_background_cloth_hook(cloth_header(cloth.bytes).constrain_hook, cloth, context);
            }
        }
        if (cloth_header(cloth.bytes).specular == 0)
        {
            shade_runtime_background_cloth(cloth); // 0x0004B800, EBX cloth
        }
        else
        {
            shade_runtime_background_cloth_specular(cloth, context.camera()); // 0x0004BA90, ESI cloth
        }
        hook = cloth_header(cloth.bytes).awake_hook;
    }
    if (hook != 0)
    {
        call_runtime_background_cloth_hook(hook, cloth, context);
    }
}

// Xbox 0x0004C0B0, EDI cloth
void settle_runtime_background_cloth(RuntimeBackgroundCloth& cloth, RuntimeBackgroundClothContext& context)
{
    for (u32 count = 1000; count != 0; --count)
    {
        step_runtime_background_cloth(cloth, 0.1f, context); // 0x0004C000
    }
}

// Xbox 0x0004C1B0, EBX cloth
float runtime_background_cloth_motion(RuntimeBackgroundCloth& cloth)
{
    const u32 count = cloth_header(cloth.bytes).points;
    if (count == 0)
    {
        return 0.f; // DAT_0032B100
    }
    float total = 0.f;
    for (u32 index = 0; index < count; ++index)
    {
        const PointGraphPoint& point = cloth.point(index);
        total = (point.position.z - point.previous.z) * (point.position.z - point.previous.z) + (point.position.x - point.previous.x) * (point.position.x - point.previous.x)
            + (point.position.y - point.previous.y) * (point.position.y - point.previous.y) + total;
    }
    long double scale = (long double)i32(count);
    if (i32(count) < 0)
    {
        scale = scale + 4294967296.0L; // DAT_0032B144
    }
    return float((long double)total / scale);
}

// Xbox 0x0004B800, EBX cloth
void shade_runtime_background_cloth(RuntimeBackgroundCloth& cloth)
{
    auto bytes = cloth.bytes;
    const float area = cloth_header(bytes).column_step * cloth_header(bytes).row_step;
    const float twice = area * 2.f; // DAT_0032B110
    const float squared = area * area;
    const float sign = cloth_header(bytes).faces_room != 0 ? -0.5f : 0.5f; // DAT_0032B1C4, DAT_0032B0F4
    const Vec3 light = cloth_header(bytes).light;
    const u32 rows = cloth_header(bytes).rows;
    const u32 columns = cloth_header(bytes).columns;
    auto* normal = reinterpret_cast<float*>(cloth.normals.data());
    auto* color = reinterpret_cast<u32*>(cloth.colors.data());
    u32 index = 0;
    for (u32 row = 0; i32(row) < i32(rows) - 1; ++row)
    {
        for (u32 column = 0; i32(column) < i32(columns) - 1; ++column)
        {
            const Vec3 origin = point_position(cloth, index);
            const Vec3 across = point_position(cloth, index + 1);
            const Vec3 down = point_position(cloth, index + columns);
            const float ax = across.x - origin.x;
            const float ay = across.y - origin.y;
            const float az = across.z - origin.z;
            const float bx = down.x - origin.x;
            const float by = down.y - origin.y;
            const float bz = down.z - origin.z;
            normal[0] = bz * ay - by * az;
            normal[1] = bx * az - bz * ax;
            normal[2] = by * ax - bx * ay;
            const float scale = twice / (normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]
                + squared);
            normal[0] = normal[0] * scale;
            normal[1] = scale * normal[1];
            normal[2] = scale * normal[2];
            const float level = (light.x * normal[0] + normal[2] * light.z + normal[1] * light.y) * sign + 0.5f;
            *color++ = grey(truncate_to_int((long double)level * 255.0L)); // DAT_0032B158, 0x001673D8
            normal += 3;
            ++index;
        }
        *color = color[-1];
        ++color;
        normal += 3;
        ++index;
    }
    for (u32 column = 0; column < columns; ++column)
    {
        *color = color[-i32(columns)];
        ++color;
    }
}

// Xbox 0x0004BA90, ESI cloth
void shade_runtime_background_cloth_specular(RuntimeBackgroundCloth& cloth, const Vec3& camera)
{
    auto bytes = cloth.bytes;
    const float area = cloth_header(bytes).column_step * cloth_header(bytes).row_step;
    const float twice = area * 2.f; // DAT_0032B110
    const float squared = area * area;
    const float sign = cloth_header(bytes).faces_room != 0 ? -0.5f : 0.5f; // DAT_0032B1C4, DAT_0032B0F4
    const u32 rows = cloth_header(bytes).rows;
    const u32 columns = cloth_header(bytes).columns;
    auto* normals = reinterpret_cast<float*>(cloth.normals.data());
    float* normal = normals;
    u32 index = 0;
    for (u32 row = 0; i32(row) < i32(rows) - 1; ++row)
    {
        for (u32 column = 0; i32(column) < i32(columns) - 1; ++column)
        {
            const Vec3 origin = point_position(cloth, index);
            const Vec3 across = point_position(cloth, index + 1);
            const Vec3 down = point_position(cloth, index + columns);
            const float ax = across.x - origin.x;
            const float ay = across.y - origin.y;
            const float az = across.z - origin.z;
            const float bx = down.x - origin.x;
            const float by = down.y - origin.y;
            const float bz = down.z - origin.z;
            normal[0] = bz * ay - by * az;
            normal[1] = bx * az - bz * ax;
            normal[2] = by * ax - bx * ay;
            const float scale = twice / (normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]
                + squared);
            normal[0] = normal[0] * scale;
            normal[1] = normal[1] * scale;
            normal[2] = normal[2] * scale;
            normal += 3;
            ++index;
        }
        normal += 3;
        ++index;
    }
    const Vec3 light = cloth_header(bytes).light;
    auto* color = reinterpret_cast<u32*>(cloth.colors.data());
    normal = normals;
    index = 0;
    for (u32 row = 0; i32(row) < i32(rows) - 1; ++row)
    {
        for (u32 column = 0; i32(column) < i32(columns) - 1; ++column)
        {
            const Vec3 point = point_position(cloth, index);
            Vec3 view{camera.x - point.x, camera.y - point.y, camera.z - point.z};
            const float length = (view.z * view.z + view.y * view.y) + view.x * view.x;
            if (length != 0.f) // DAT_0032B100
            {
                const long double scale = retail_reciprocal_sqrt(length); // 0x001534C0
                view = {float(view.x * scale), float(view.y * scale), float(view.z * scale)};
            }
            const float level = ((normal[2] * light.z + normal[1] * light.y) + light.x * normal[0]) * sign + 0.5f;
            if (row > 2 && i32(row) < i32(rows) - 3 && column > 2 && i32(column) < i32(columns) - 3)
            {
                const float* previous = normal - 3;
                const float* next = normal + 3;
                const float* above = normal - size_t(columns) * 3;
                const float* below = normal + size_t(columns) * 3;
                const float nx = ((((previous[0] + normal[0]) + next[0]) + above[0]) + below[0]) * 0.2f; // DAT_0032B1AC
                const float ny = ((((previous[1] + normal[1]) + next[1]) + above[1]) + below[1]) * 0.2f;
                const float nz = ((((previous[2] + normal[2]) + next[2]) + above[2]) + below[2]) * 0.2f;
                const long double toward = ((long double)nz * view.z + (long double)ny * view.y)
                    + (long double)nx * view.x;
                const long double lit = ((long double)nx * light.x + (long double)nz * light.z)
                    + (long double)ny * light.y;
                const long double facing = ((long double)view.x * light.x + (long double)view.z * light.z)
                    + (long double)view.y * light.y;
                const long double reflection = toward * lit * 2.0L - facing;
                float shine = float(std::pow(reflection, 32.0L) * 2.0L + level); // DAT_0032B3C8, 0x00167CB8
                if (shine > 1.f) // DAT_0032B0F8
                {
                    shine = 1.f;
                }
                const u32 bright = u32(truncate_to_int((long double)shine * 255.0L)) & 0xFFu; // DAT_0032B158
                const u32 grey_level = u32(truncate_to_int((long double)level * 255.0L)); // 0x001673D8
                *color = ((((grey_level | 0xFFFFFF00u) << 8) | bright) << 8) | bright;
            }
            else
            {
                *color = grey(truncate_to_int((long double)level * 255.0L)); // DAT_0032B158, 0x001673D8
            }
            ++color;
            normal += 3;
            ++index;
        }
        *color = color[-1];
        ++color;
        normal += 3;
        ++index;
    }
    for (u32 column = 0; column < columns; ++column)
    {
        *color = color[-i32(columns)];
        ++color;
    }
}

// Xbox 0x0004FA10
int compare_runtime_cloth_corners(const void* left, const void* right)
{
    const float first = static_cast<const float*>(left)[1];
    const float second = static_cast<const float*>(right)[1];
    if (first == second)
    {
        return 0;
    }
    if (second <= first && first != second)
    {
        return -1;
    }
    return 1;
}

// Xbox 0x0004FA40, EAX quad, ESI output
void order_runtime_cloth_quad(const float* quad, float* output)
{
    std::memcpy(output, quad, 12 * sizeof(float));
    runtime_quick_sort(output, 4, 0xC, compare_runtime_cloth_corners); // 0x00166780
    const double third = std::sqrt(double(output[0] - output[6]) * double(output[0] - output[6])
        + double(output[1] - output[7]) * double(output[1] - output[7])
        + double(output[2] - output[8]) * double(output[2] - output[8]));
    const double fourth = std::sqrt(double(output[0] - output[9]) * double(output[0] - output[9])
        + double(output[1] - output[10]) * double(output[1] - output[10])
        + double(output[2] - output[11]) * double(output[2] - output[11]));
    if (third < fourth)
    {
        for (u32 axis = 0; axis < 3; ++axis)
        {
            std::swap(output[6 + axis], output[9 + axis]);
        }
    }
}

// Xbox 0x0004FB30, EAX part table, EDI quad
RuntimeBackgroundCloth* build_runtime_background_cloth(u16 rows, u16 columns, u16 edges, std::span<const u8> part,
    u16 pin_step, const Vec3& gravity, std::span<const u8> part_table, const float* quad,
    RuntimeBackgroundClothContext& context)
{
    const float ax = quad[3] - quad[0];
    const float ay = quad[4] - quad[1];
    const float az = quad[5] - quad[2];
    const float bx = quad[9] - quad[0];
    const float by = quad[10] - quad[1];
    const float bz = quad[11] - quad[2];
    Vec3 normal{bz * ay - by * az, az * bx - bz * ax, by * ax - ay * bx};
    normalize_runtime_graphics_vector(normal, normal); // 0x00184F99
    RuntimeBackgroundCloth* cloth = create_runtime_background_cloth(rows, columns, edges, context); // 0x0004B5F0
    auto bytes = cloth->bytes;
    cloth_header(bytes).unk_1e5 = u8(3);
    cloth_header(bytes).texture = 0u;
    const u16 texture = read_u16(part, 0x32);
    if (u32(texture) < read_u32(part_table, 8))
    {
        cloth_header(bytes).texture = read_u32(context.resolve(read_u32(part_table, 0xC) + u32(texture) * 4, 4), 0);
    }
    const u32 count_rows = cloth_header(bytes).rows;
    const u32 count_columns = cloth_header(bytes).columns;
    for (u32 row = 0; row < count_rows; ++row)
    {
        for (u32 column = 0; column < count_columns; ++column)
        {
            const long double wave = std::sin((long double)float(row)
                / ((long double)i32(count_rows) * (long double)0.33333334f) * (long double)6.2831855f) * 2.0L;
            const float across = float(row) / float(i32(count_rows - 1));
            const float down = float(column) / float(i32(count_columns - 1));
            PointGraphPoint& point = cloth->point(row * count_columns + column);
            point.position.x = float((long double)normal.x * wave) + ((quad[0] + ax * across) + bx * down);
            point.position.y = ((quad[1] + ay * across) + by * down) + float((long double)normal.y * wave);
            point.position.z = ((quad[2] + az * across) + bz * down) + float((long double)normal.z * wave);
            point.pinned = 0;
            point.force.x = gravity.x;
            point.force.y = gravity.y;
            point.force.z = gravity.z;
            point.inverse_mass = 1.f; // DAT_0032B0F8
        }
    }
    const float per_row = 1.f / float(count_rows);
    const float row_x = per_row * ax;
    const float row_y = ay * per_row;
    const float row_z = az * per_row;
    const float per_column = 1.f / float(count_columns);
    const float column_x = per_column * bx;
    const float column_y = by * per_column;
    const float column_z = bz * per_column;
    cloth_header(bytes).row_step = float(std::sqrt(double(row_y * row_y + row_z * row_z + row_x * row_x)));
    cloth_header(bytes).column_step = float(std::sqrt(double(column_y * column_y + column_z * column_z + column_x * column_x)));
    for (u32 row = 0; row < count_rows; row += pin_step)
    {
        cloth->graph.point(row * count_columns).pinned = 1;
    }
    cloth->graph.point((count_rows - 1) * count_columns).pinned = 1;
    for (u32 index = 0; index < cloth_header(bytes).points; ++index)
    {
        PointGraphPoint& point = cloth->point(index);
        point.previous.x = point.position.x;
        point.previous.y = point.position.y;
        point.previous.z = point.position.z;
    }
    const auto add_edge = [&](u32 first, u32 second)
    {
        const PointGraphPoint& a = cloth->point(first);
        const PointGraphPoint& b = cloth->point(second);
        const float dx = a.position.x - b.position.x;
        const float dy = a.position.y - b.position.y;
        const float dz = a.position.z - b.position.z;
        const float length = float(std::sqrt(double(dy * dy + dz * dz + dx * dx)));
        if (cloth_header(bytes).sorted == 0)
        {
            const u32 edge = cloth_header(bytes).edges;
            PointGraphEdge& row = cloth->graph.edge(edge);
            row.first = first;
            row.second = second;
            const float squared = length * length;
            const float inverse = 1.f / squared;
            row.length = length;
            row.inverse_squared_length = inverse;
            row.squared_length = squared;
            cloth_header(bytes).edges = edge + 1;
        }
    };
    for (u32 row = 0; row < count_rows; ++row)
    {
        for (u32 column = 0; column < count_columns; ++column)
        {
            const u32 index = row * count_columns + column;
            if (i32(row) < i32(count_rows) - 1)
            {
                add_edge(index, index + count_columns);
            }
            if (i32(column) < i32(count_columns) - 1)
            {
                add_edge(index, index + 1);
            }
        }
    }
    prepare_runtime_point_graph_edges(cloth->graph); // 0x001024A0
    Vec3 center{quad[3] + quad[0], quad[4] + quad[1], quad[5] + quad[2]};
    center = {center.x + quad[6], center.y + quad[7], center.z + quad[8]};
    center = {center.x + quad[9], center.y + quad[10], center.z + quad[11]};
    center = {center.x * 0.25f, center.y * 0.25f, center.z * 0.25f}; // DAT_0032B104
    cloth_header(bytes).center = center;
    cloth_header(bytes).radius = float(std::sqrt(double((center.y - quad[1]) * (center.y - quad[1])
        + (center.z - quad[2]) * (center.z - quad[2]) + (center.x - quad[0]) * (center.x - quad[0]))));
    cloth_header(bytes).normal = normal;
    cloth_header(bytes).always_stepped = 1u;
    return cloth;
}

// Xbox 0x00050CC0, EBX part
void build_runtime_background_curtains(std::span<const u8> part, std::span<const u8> part_table,
    RuntimeBackgroundCloth*& first, RuntimeBackgroundCloth*& second, RuntimeBackgroundClothContext& context)
{
    const Vec3 gravity{0.f, -2.5f, 0.f}; // DAT_0032B4E4
    std::array<float, 12> quad;
    order_runtime_cloth_quad(reinterpret_cast<const float*>(part.data()), quad.data()); // 0x0004FA40
    const Vec3 q0{quad[0], quad[1], quad[2]};
    const Vec3 q1{quad[3], quad[4], quad[5]};
    const Vec3 q2{quad[6], quad[7], quad[8]};
    const Vec3 q3{quad[9], quad[10], quad[11]};
    const Vec3 rail{q1.x - q0.x, q1.y - q0.y, q1.z - q0.z};
    const Vec3 hem{q2.x - q3.x, q2.y - q3.y, q2.z - q3.z};
    Vec3 normal{(q2.z - q1.z) * (q1.y - q0.y) - (q2.y - q1.y) * (q1.z - q0.z),
        (q1.z - q0.z) * (q2.x - q1.x) - (q2.z - q1.z) * (q1.x - q0.x),
        (q2.y - q1.y) * (q1.x - q0.x) - (q1.y - q0.y) * (q2.x - q1.x)};
    const float length = normal.z * normal.z + normal.y * normal.y + normal.x * normal.x;
    if (length != 0.f) // DAT_0032B100
    {
        const long double scale = (long double)retail_reciprocal_sqrt(length); // 0x001534C0
        normal = {float(normal.x * scale), float(normal.y * scale), float(normal.z * scale)};
    }
    constexpr float kNear = 0.55f; // DAT_0032B52C
    constexpr float kFar = 0.45f; // DAT_0032B4F8
    const Vec3 a{rail.x * kNear + q0.x, rail.y * kNear + q0.y, rail.z * kNear + q0.z};
    const Vec3 b{rail.x * kFar + q0.x, rail.y * kFar + q0.y, rail.z * kFar + q0.z};
    const Vec3 c{q3.x + hem.x * kNear, q3.y + hem.y * kNear, q3.z + hem.z * kNear};
    const Vec3 d{q3.x + hem.x * kFar, q3.y + hem.y * kFar, q3.z + hem.z * kFar};
    const std::array<float, 12> left{q0.x, q0.y, q0.z, a.x, a.y, a.z, c.x, c.y, c.z, q3.x, q3.y, q3.z};
    const std::array<float, 12> right{b.x + normal.x, b.y + normal.y, b.z + normal.z, q1.x + normal.x,
        q1.y + normal.y, q1.z + normal.z, q2.x + normal.x, q2.y + normal.y, q2.z + normal.z, d.x + normal.x,
        d.y + normal.y, d.z + normal.z};
    first = build_runtime_background_cloth(0x11, 0x10, 0x1FF, part, 4, gravity, part_table, left.data(),
        context); // 0x0004FB30
    second = build_runtime_background_cloth(0x11, 0x10, 0x1FF, part, 4, gravity, part_table, right.data(),
        context); // 0x0004FB30
}

// Xbox 0x00050880, EAX part, EBX model
RuntimeBackgroundCloth* build_runtime_background_flag(std::span<const u8> part, std::span<const u8> part_table,
    RuntimeBackgroundClothContext& context)
{
    const u8 kind = part[0x30];
    u16 pin_step;
    if (kind == 2 || kind == 8)
    {
        pin_step = 5;
    }
    else if (kind == 1)
    {
        pin_step = 0x13;
    }
    else
    {
        return nullptr;
    }
    const Vec3 gravity{0.f, -2.5f, 0.f}; // DAT_0032B4E4
    std::array<float, 12> quad;
    order_runtime_cloth_quad(reinterpret_cast<const float*>(part.data()), quad.data()); // 0x0004FA40
    return build_runtime_background_cloth(0x14, 0x10, 0x25C, part, pin_step, gravity, part_table, quad.data(),
        context); // 0x0004FB30
}

// Xbox 0x000509E0, the part on the stack, EBP the output
u32 build_runtime_background_strips(std::span<const u8> part, std::span<const u8> part_table,
    std::array<RuntimeBackgroundCloth*, 6>& output, RuntimeBackgroundClothContext& context)
{
    const Vec3 gravity{0.f, -10.5f, 0.f}; // DAT_0032B530
    std::array<float, 12> corners;
    order_runtime_cloth_quad(reinterpret_cast<const float*>(part.data()), corners.data()); // 0x0004FA40
    const Vec3 across{corners[3] - corners[0], corners[4] - corners[1], corners[5] - corners[2]};
    const Vec3 down{corners[9] - corners[0], corners[10] - corners[1], corners[11] - corners[2]};
    constexpr float sixth = std::bit_cast<float>(0x3E2AAAABu); // DAT_0032B208
    constexpr float gap = std::bit_cast<float>(0x3D4CCCCDu); // DAT_0032B250
    const Vec3 step{across.x * sixth, across.y * sixth, across.z * sixth};
    const Vec3 margin{across.x * gap, across.y * gap, across.z * gap};
    for (u32 index = 0; index < 6; ++index)
    {
        const float first = float(index);
        const float second = float(index + 1);
        const Vec3 left{step.x * first + corners[0] + margin.x, step.y * first + corners[1] + margin.y,
            step.z * first + corners[2] + margin.z};
        const Vec3 right{step.x * second + corners[0] - margin.x, step.y * second + corners[1] - margin.y,
            step.z * second + corners[2] - margin.z};
        const std::array<float, 12> quad{left.x, left.y, left.z, right.x, right.y, right.z, right.x + down.x,
            right.y + down.y, right.z + down.z, left.x + down.x, left.y + down.y, left.z + down.z};
        output[index] = build_runtime_background_cloth(2, 0x10, 0x114, part, 1, gravity, part_table, quad.data(),
            context); // 0x0004FB30, EAX the part table, EDI the quad
        if (output[index] == nullptr)
        {
            throw Error("0x000509E0 writes a strip curtain that 0x0004FB30 did not make");
        }
        cloth_header(output[index]->bytes).rustles = u8(1);
        std::strcpy(cloth_header(output[index]->bytes).sound.data(), "XACT_SOUNDBANK_GPROPS_PLASTICCURTAINS");
    }
    return 6;
}

// Xbox 0x00050220, ECX size, EAX part table, EDI quad
RuntimeBackgroundCloth* build_runtime_background_water_sheet(u16 size, u16 edges, std::span<const u8> part,
    std::span<const u8> part_table, RuntimeBackgroundClothContext& context)
{
    const float* quad = reinterpret_cast<const float*>(part.data());
    const float az = quad[5] - quad[2];
    const float bz = quad[11] - quad[2];
    const float ax = quad[3] - quad[0];
    const float bx = quad[9] - quad[0];
    Vec3 normal{bz * 0.f - az * 0.f, az * bx - bz * ax, ax * 0.f - bx * 0.f};
    normalize_runtime_graphics_vector(normal, normal); // 0x00184F99
    RuntimeBackgroundCloth* cloth = create_runtime_background_cloth(size, size, edges, context); // 0x0004B5F0
    auto bytes = cloth->bytes;
    cloth_header(bytes).unk_1e5 = u8(3);
    cloth_header(bytes).texture = 0u;
    const u16 texture = read_u16(part, 0x32);
    if (u32(texture) < read_u32(part_table, 8))
    {
        cloth_header(bytes).texture = read_u32(context.resolve(read_u32(part_table, 0xC) + u32(texture) * 4, 4), 0);
    }
    const u32 rows = cloth_header(bytes).rows;
    const u32 columns = cloth_header(bytes).columns;
    for (u32 row = 0; row < rows; ++row)
    {
        for (u32 column = 0; column < columns; ++column)
        {
            const float across = float(i32(row)) / float(i32(rows - 1));
            const float down = float(i32(column)) / float(i32(columns - 1));
            PointGraphPoint& point = cloth->point(row * columns + column);
            point.position.x = (quad[0] + ax * across) + bx * down;
            point.position.y = (quad[1] + across * 0.f) + down * 0.f;
            point.position.z = (quad[2] + az * across) + bz * down;
            const bool edge = row == 0 || i32(rows - 1) <= i32(row) || column == 0 || i32(columns - 1) <= i32(column);
            point.pinned = edge ? 1 : 0;
            point.force.x = context.origin.x; // DAT_0050DD80
            point.force.y = context.origin.y;
            point.force.z = context.origin.z;
            point.inverse_mass = 1.f; // DAT_0032B0F8
        }
    }
    const float per_row = 1.f / float(rows);
    const float per_column = 1.f / float(columns);
    const float row_x = ax * per_row;
    const float row_y = per_row * 0.f;
    const float row_z = az * per_row;
    const float column_x = per_column * bx;
    const float column_y = per_column * 0.f;
    const float column_z = bz * per_column;
    cloth_header(bytes).row_step = float(std::sqrt(((long double)row_z * row_z + (long double)row_y * row_y)
        + (long double)row_x * row_x));
    cloth_header(bytes).column_step = float(std::sqrt(((long double)column_z * column_z
        + (long double)column_y * column_y) + (long double)column_x * column_x));
    for (u32 index = 0; index < cloth_header(bytes).points; ++index)
    {
        PointGraphPoint& point = cloth->point(index);
        point.previous.x = point.position.x;
        point.previous.y = point.position.y;
        point.previous.z = point.position.z;
    }
    // `vertical` sums the squares z, y, x (the edge to the next row), else x, z, y.
    const auto add_edge = [&](u32 first, u32 second, bool vertical)
    {
        const PointGraphPoint& a = cloth->point(first);
        const PointGraphPoint& b = cloth->point(second);
        const long double dx = (long double)a.position.x - b.position.x;
        const long double dy = (long double)a.position.y - b.position.y;
        const long double dz = (long double)a.position.z - b.position.z;
        const float length = float(std::sqrt(vertical ? (dz * dz + dy * dy) + dx * dx : (dx * dx + dz * dz) + dy * dy));
        if (cloth_header(bytes).sorted == 0)
        {
            const u32 edge = cloth_header(bytes).edges;
            PointGraphEdge& row = cloth->graph.edge(edge);
            row.first = first;
            row.second = second;
            const float squared = length * length;
            const float inverse = 1.f / squared; // DAT_0032B0F8
            row.length = length;
            row.inverse_squared_length = inverse;
            row.squared_length = squared;
            cloth_header(bytes).edges = edge + 1;
        }
    };
    for (u32 row = 0; row < rows; ++row)
    {
        for (u32 column = 0; column < columns; ++column)
        {
            const u32 index = row * columns + column;
            if (i32(row) < i32(rows) - 1)
            {
                add_edge(index, index + columns, true);
            }
            if (i32(column) < i32(columns) - 1)
            {
                add_edge(index, index + 1, false);
            }
        }
    }
    prepare_runtime_point_graph_edges(cloth->graph); // 0x001024A0
    Vec3 center{quad[3] + quad[0], quad[4] + quad[1], quad[5] + quad[2]};
    center = {center.x + quad[6], center.y + quad[7], center.z + quad[8]};
    center = {center.x + quad[9], center.y + quad[10], center.z + quad[11]};
    center = {center.x * 0.25f, center.y * 0.25f, center.z * 0.25f}; // DAT_0032B104
    cloth_header(bytes).center = center;
    cloth_header(bytes).radius = float(std::sqrt((long double)(center.y - quad[1]) * (center.y - quad[1])
        + (long double)(center.z - quad[2]) * (center.z - quad[2])
        + (long double)(center.x - quad[0]) * (center.x - quad[0])));
    cloth_header(bytes).normal = normal;
    cloth_header(bytes).always_stepped = 1u;
    return cloth;
}

// Xbox 0x00050940, EAX model, ECX part
RuntimeBackgroundCloth* build_runtime_background_water(std::span<const u8> part, std::span<const u8> part_table,
    RuntimeBackgroundClothContext& context)
{
    RuntimeBackgroundCloth* cloth = build_runtime_background_water_sheet(0x1C, 0x5E8, part, part_table,
        context); // 0x00050220
    if (cloth != nullptr)
    {
        cloth_header(cloth->bytes).rustles = u8(1);
        const char* sound = "XACT_SOUNDBANK_GFOOTSTEPWATER_DEFAULT"; // 0x002FE740
        char* output = reinterpret_cast<char*>(cloth_header(cloth->bytes).sound.data());
        char character;
        do
        {
            character = *sound++;
            *output++ = character;
        } while (character != 0);
    }
    return cloth;
}

// Xbox 0x00114580
void level_runtime_background_water(RuntimeBackgroundCloth& cloth)
{
    const float level = cloth.point(0).position.y;
    for (u32 index = 0; index < cloth_header(cloth.bytes).points; index = (index + 1) & 0xFFFF)
    {
        PointGraphPoint& point = cloth.point(index);
        const float offset = level - point.position.y;
        const float pull = offset * offset * 0.2f; // DAT_0032B1AC
        point.force.y = pull;
        if (point.position.y > level)
        {
            point.force.y = 0.f - pull;
        }
    }
}

// Xbox 0x001145F0
void ripple_runtime_background_water(RuntimeBackgroundCloth& cloth, RuntimeBackgroundClothContext& context)
{
    auto bytes = cloth.bytes;
    const Vec3 bounds_low = cloth_header(bytes).low;
    const Vec3 bounds_high = cloth_header(bytes).high;
    const float middle = (bounds_high.y + bounds_low.y) * 0.5f; // DAT_0032B0F4
    const Vec3 low{bounds_low.x, bounds_low.y - 30.f, bounds_low.z}; // DAT_0032B108
    const Vec3 high{bounds_high.x, bounds_high.y + 30.f, bounds_high.z};
    for (const void* node = context.wide_first; node != nullptr;
         node = static_cast<const entity_detail::ListHead*>(node)->next)
    {
        const Actor& actor = *context.wide_actor(node); // The node less 0x1E8.
        const Vec3 position = actor_world_position(actor); // 0x000412A0
        if (low.x > position.x || position.x > high.x || low.y > position.y || position.y > high.y
            || low.z > position.z || position.z > high.z)
        {
            continue;
        }
        Vec3 foot = actor_world_position(actor); // 0x000412A0
        const float depth = engine_absolute(float(((long double)actor.model_size_y - actor.model_size_z)
            * actor.radius_y)); // 0x00152BB0
        foot.y = actor.radius_y * actor.model_size_z + foot.y;
        (void)engine_absolute(actor.model_size_x * actor.radius_xz); // 0x00152BB0; the result is dropped.
        if (!(middle > foot.y) || !(foot.y + depth > middle))
        {
            continue;
        }
        const float surface = middle + 3.4f; // DAT_0032B4DC
        const u32 rows = cloth_header(bytes).rows;
        for (u32 row = 1; i32(row) < i32(rows) - 1; row = (row + 1) & 0xFFFF)
        {
            for (u32 column = 1; i32(column) < i32(cloth_header(bytes).columns) - 1; column = (column + 1) & 0xFFFF)
            {
                const float radius = float(std::sin((long double)context.scene_time * 5.0L) * (long double)0.4f
                    + (long double)4.4f); // DAT_004312CC, DAT_0032B184, DAT_0032B11C, DAT_0032B4D8
                PointGraphPoint& point = cloth.point(cloth_header(bytes).columns * row + column);
                const float dx = point.position.x - position.x;
                const float dy = point.position.y - surface;
                const float dz = point.position.z - position.z;
                if (!(radius * radius > (dy * dy + dx * dx) + dz * dz))
                {
                    continue;
                }
                point.position.x = dx;
                point.position.y = dy;
                point.position.z = dz;
                if (dx == 0.f && dy == 0.f && dz == 0.f)
                {
                    continue;
                }
                Vec3 away{dx, dy, dz};
                normalize_runtime_graphics_vector(away, away); // 0x00184F99, in place
                point.position.x = away.x * radius + position.x;
                point.position.y = away.y * radius + surface;
                point.position.z = away.z * radius + position.z;
            }
        }
    }
}

// Xbox 0x001025E0
bool push_runtime_point_from_capsule(PointGraphPoint& point, const Vec3& start, const Vec3& end, float radius)
{
    const float dx = start.x - end.x;
    const float dz = start.z - end.z;
    const float dy = start.y - end.y;
    const float squared = (dz * dz + dx * dx) + dy * dy;
    if (squared == 0.f)
    {
        return false;
    }
    const Vec3 p{point.position.x, point.position.y, point.position.z};
    const float along = (((start.x * start.x + start.y * start.y) + start.z * start.z)
        - ((p.z * dz + p.y * dy) + p.x * dx)
        - ((start.x * end.x + end.z * start.z) + end.y * start.y)) / squared;
    if (along < 0.f || 1.f < along)
    {
        return false;
    }
    const float back = 0.f - along;
    const Vec3 nearest{start.x + dx * back, start.y + dy * back, start.z + dz * back};
    const float qz = nearest.z - p.z;
    const float qy = nearest.y - p.y;
    const float qx = nearest.x - p.x;
    if (!(radius * radius > (qz * qz + qy * qy) + qx * qx))
    {
        return false;
    }
    const Vec3& tip = along <= 0.5f ? end : start; // DAT_0032B0F4, CMOVBE
    const float ex = tip.x - nearest.x;
    const float ey = tip.y - nearest.y;
    const float ez = tip.z - nearest.z;
    const float reach = float((long double)radius - runtime_point_distance(p, nearest)); // 0x00146CE0
    if (reach * reach > (ez * ez + ey * ey) + ex * ex)
    {
        point.position.x = p.x + ex;
        point.position.y = p.y + ey;
        point.position.z = p.z + ez;
        return true;
    }
    Vec3 away{p.x - nearest.x, p.y - nearest.y, p.z - nearest.z};
    normalize_runtime_graphics_vector(away, away); // 0x00184F99
    point.position.x = away.x * radius + nearest.x;
    point.position.y = away.y * radius + nearest.y;
    point.position.z = radius * away.z + nearest.z;
    return true;
}

// Xbox 0x00102860
void push_runtime_cloth_point_from_sphere(RuntimeBackgroundCloth& cloth, u32 index, const Vec3& center, float radius)
{
    PointGraphPoint& point = cloth.point(index);
    const float dy = point.position.y - center.y;
    const float dx = point.position.x - center.x;
    const float dz = point.position.z - center.z;
    if (!(dx * dx + dy * dy + dz * dz < radius * radius))
    {
        return;
    }
    point.position.x = dx;
    point.position.y = point.position.y - center.y;
    const float z = point.position.z - center.z;
    point.position.z = z;
    if (dx == 0.f && point.position.y == 0.f && z == 0.f)
    {
        return;
    }
    Vec3 away{point.position.x, point.position.y, point.position.z};
    normalize_runtime_graphics_vector(away, away); // 0x00184F99
    point.position.x = center.x + away.x * radius;
    point.position.y = center.y + away.y * radius;
    point.position.z = center.z + away.z * radius;
}

namespace
{
// 0x000426F0 into the record, then its start, end and the push radius (0.6 of its radius, DAT_0032B248).
void build_capsule(const Actor& actor, std::array<u8, 0x40>& record, RuntimeBackgroundClothContext& context)
{
    context.actor_bound(actor, record.data()); // 0x000426F0, mask -1
    const Vec3 start{read_f32(record, 4), read_f32(record, 8), read_f32(record, 0xC)};
    const Vec3 axis{read_f32(record, 0x10), read_f32(record, 0x14), read_f32(record, 0x18)};
    const float values[7]{start.x, start.y, start.z, axis.x + start.x, axis.y + start.y, axis.z + start.z,
        read_f32(record, 0x1C) * 0.6f};
    std::memcpy(record.data() + 0x20, values, sizeof(values));
}

Vec3 capsule_vec(const std::array<u8, 0x40>& record, size_t offset)
{
    return {read_f32(record, offset), read_f32(record, offset + 4), read_f32(record, offset + 8)};
}

// Each collision plane the point lies behind moves it onto the plane. `y_first` sums the distance y, z, x (the
// pushing pass), else x, y, z.
void push_from_planes(RuntimeBackgroundCloth& cloth, u32 index, bool y_first)
{
    PointGraphPoint& point = cloth.point(index);
    for (const auto& plane : cloth.planes)
    {
        const float distance = y_first
            ? point.position.y * plane[1] + point.position.z * plane[2] + point.position.x * plane[0] + plane[3]
            : point.position.x * plane[0] + point.position.y * plane[1] + point.position.z * plane[2] + plane[3];
        if (distance < 0.f)
        {
            const float depth = 0.f - distance;
            point.position.x = depth * plane[0] + point.position.x;
            point.position.y = depth * plane[1] + point.position.y;
            point.position.z = depth * plane[2] + point.position.z;
        }
    }
}
}

// Xbox 0x00114910
void constrain_runtime_background_cloth(RuntimeBackgroundCloth& cloth, RuntimeBackgroundClothContext& context)
{
    auto bytes = cloth.bytes;
    const Vec3 low = cloth_header(bytes).low;
    const Vec3 high = cloth_header(bytes).high;
    const Vec3 center{(high.x + low.x) * 0.5f, (high.y + low.y) * 0.5f, (high.z + low.z) * 0.5f}; // DAT_0032B0F4
    bool player_near = false;
    if (const Actor* player = context.player(); player != nullptr)
    {
        const Vec3 position = actor_world_position(*player); // 0x000412A0
        const float dz = position.z - center.z;
        const float dy = position.y - center.y;
        const float dx = position.x - center.x;
        if (3600.f > (dz * dz + dy * dy) + dx * dx) // DAT_0032B410
        {
            player_near = true;
            build_capsule(*player, context.player_capsule, context);
        }
    }
    const Actor* nearest = nullptr;
    float distance = 0.f;
    const bool other_near = find_runtime_nearest_wide_actor(center, nearest, distance, true, context); // 0x00114490
    if (other_near)
    {
        build_capsule(*nearest, context.nearest_capsule, context);
    }
    const u32 count = u32(cloth_header(bytes).columns) * u32(cloth_header(bytes).rows);
    if (!player_near && !other_near)
    {
        for (u32 index = 0; i32(index) < i32(count); index = (index + 1) & 0xFFFF)
        {
            if (cloth.graph.point(index).pinned == 0)
            {
                push_from_planes(cloth, index, false);
            }
        }
        return;
    }
    for (u32 index = 0; i32(index) < i32(count); index = (index + 1) & 0xFFFF)
    {
        if (cloth.graph.point(index).pinned != 0)
        {
            continue;
        }
        if (other_near)
        {
            push_runtime_point_from_capsule(cloth.point(index), capsule_vec(context.nearest_capsule, 0x20),
                capsule_vec(context.nearest_capsule, 0x2C), read_f32(context.nearest_capsule, 0x38)); // 0x001025E0
        }
        if (player_near)
        {
            const Vec3 end = capsule_vec(context.player_capsule, 0x2C);
            const float radius = read_f32(context.player_capsule, 0x38);
            push_runtime_point_from_capsule(cloth.point(index), capsule_vec(context.player_capsule, 0x20), end,
                radius); // 0x001025E0
            push_runtime_cloth_point_from_sphere(cloth, index, end, radius); // 0x00102860, EDI the end
        }
        push_from_planes(cloth, index, true);
    }
}

void call_runtime_background_cloth_hook(u32 hook, RuntimeBackgroundCloth& cloth,
    RuntimeBackgroundClothContext& context)
{
    switch (hook)
    {
    case 0x001151F0:
        settle_when_still(cloth);
        return;
    case 0x001152C0:
        wake_when_near(cloth, context);
        return;
    case 0x00114910:
        constrain_runtime_background_cloth(cloth, context);
        return;
    case 0x00114580:
        level_runtime_background_water(cloth);
        return;
    case 0x001145F0:
        ripple_runtime_background_water(cloth, context);
        return;
    default:
    {
        char text[64];
        std::snprintf(text, sizeof(text), "background cloth hook 0x%08X is not translated", hook);
        throw Error(text);
    }
    }
}

// Xbox 0x00114490
bool find_runtime_nearest_wide_actor(const Vec3& point, const Actor*& found, float& distance, bool skip_player,
    RuntimeBackgroundClothContext& context)
{
    const Actor* player = context.player();
    const Actor* best = nullptr;
    float nearest = 0.f;
    for (const void* node = context.wide_first; node != nullptr;
         node = static_cast<const entity_detail::ListHead*>(node)->next)
    {
        const Actor* actor = context.wide_actor(node);
        if (actor == nullptr || (skip_player && actor == player))
        {
            continue;
        }
        const Vec3 position = actor_world_position(*actor); // 0x000412A0
        const float dx = position.x - point.x;
        const float dy = position.y - point.y;
        const float dz = position.z - point.z;
        const float squared = dz * dz + dx * dx + dy * dy;
        if (best == nullptr || nearest > squared)
        {
            nearest = squared;
            best = actor;
        }
    }
    found = best;
    if (best == nullptr)
    {
        return false;
    }
    distance = nearest;
    return 2500.f > nearest; // DAT_0032B40C
}

// Xbox 0x00114090, ECX cloth
bool runtime_background_cloth_in_view(RuntimeBackgroundCloth& cloth, std::span<const float, 42> frustum, u8& hint)
{
    auto bytes = cloth.bytes;
    const Vec3 low = cloth_header(bytes).low;
    const Vec3 high = cloth_header(bytes).high;
    const Vec3 center{(low.x + high.x) * 0.5f, (low.y + high.y) * 0.5f, (low.z + high.z) * 0.5f};
    cloth_header(bytes).view_high = Vec3{center.x + 60.f, center.y + 60.f, center.z + 60.f}; // DAT_0032B28C
    cloth_header(bytes).view_low = Vec3{center.x - 60.f, center.y - 60.f, center.z - 60.f};
    return classify_runtime_box_frustum(frustum, cloth_header(bytes).view_low, cloth_header(bytes).view_high,
        hint) != 0; // 0x00159BA0
}
}
