#include "physical_mesh_queries.h"
#include "gbtg/model_nodes.h"
#include "gbtg/anim.h"
#include "contact_sweep.h"
#include "mesh_sphere_geometry.h"
#include <bit>
#include <cstring>
#include <xmmintrin.h>

namespace gbtg
{
// Xbox 0x000544B0
void runtime_mesh_pose_matrices(const ModelPoseBuffer* pose, i32 joint,
    const float* world, const float* inverse, float* output_world, float* output_inverse)
{
    if (pose != nullptr)
    {
        const float* bone = model_pose_matrix(*pose, joint).data();
        invert_model_matrix(output_inverse, bone);
        multiply_affine_matrices(output_inverse, output_inverse, inverse);
        multiply_affine_matrices(output_world, world, bone);
        return;
    }
    std::memcpy(output_inverse, inverse, 48);
    std::memcpy(output_world, world, 48);
}

namespace
{
// A grid mesh's header and its cell table, sized from its cell bounds.
std::span<const u8> resolve_grid_mesh(u32 address, const std::function<std::span<const u8>(u32, size_t)>& resolve)
{
    const auto header = resolve(address, model_grid_mesh_field::cells.offset);
    const auto low = get(header, model_grid_mesh_field::cell_min);
    const auto high = get(header, model_grid_mesh_field::cell_max);
    const u32 x = u32(i32(std::bit_cast<i16>(high[0])) - std::bit_cast<i16>(low[0]));
    const u32 y = u32(i32(std::bit_cast<i16>(high[1])) - std::bit_cast<i16>(low[1]));
    const u32 z = u32(i32(std::bit_cast<i16>(high[2])) - std::bit_cast<i16>(low[2]));
    const u32 cells = z * get(header, model_grid_mesh_field::plane_stride) +
        y * get(header, model_grid_mesh_field::row_stride) + x + 1;
    return resolve(address, model_grid_mesh_field::cells.offset + size_t(cells) * model_grid_mesh_field::cells.stride);
}
}

void bind_runtime_physical_mesh_methods(
    std::unordered_map<u32, std::function<u32(RuntimeCollisionShapeCall&)>>& methods,
    std::function<std::span<const u8>(u32, size_t)> resolve)
{
    methods[0x53720] = [resolve](RuntimeCollisionShapeCall& call)
    {
        return u32(collide_runtime_physical_mesh_segment(call, resolve_grid_mesh(call.shape_address, resolve),
            resolve));
    };
    methods[0x53AA0] = [resolve](RuntimeCollisionShapeCall& call)
    {
        return u32(collide_runtime_physical_mesh_sweep(call, resolve_grid_mesh(call.shape_address, resolve), resolve));
    };
    methods[0x53F20] = [resolve](RuntimeCollisionShapeCall& call)
    {
        return u32(collect_runtime_physical_mesh_contacts(call, resolve_grid_mesh(call.shape_address, resolve),
            resolve));
    };
    const auto pose_of = [](const RuntimeCollisionShapeCall& call)
    {
        if (call.pose == nullptr)
        {
            return static_cast<const ModelPoseBuffer*>(nullptr);
        }
        if (call.pose->buffer == nullptr)
        {
            throw Error("physical mesh collision requires its native model pose buffer");
        }
        return call.pose->buffer;
    };
    methods[0x54520] = [resolve, pose_of](RuntimeCollisionShapeCall& call)
    {
        return u32(collide_runtime_posed_mesh_segment(call, pose_of(call), resolve(call.shape_address, 0x1C), resolve));
    };
    methods[0x54850] = [resolve, pose_of](RuntimeCollisionShapeCall& call)
    {
        return u32(collide_runtime_posed_mesh_sweep(call, pose_of(call), resolve(call.shape_address, 0x1C), resolve));
    };
    methods[0x54C40] = [resolve, pose_of](RuntimeCollisionShapeCall& call)
    {
        return u32(collect_runtime_posed_mesh_contacts(call, pose_of(call), resolve(call.shape_address, 0x1C), resolve));
    };
}

namespace
{
i32 wrap_add(i32 first, i32 second)
{
    return std::bit_cast<i32>(u32(first) + u32(second));
}
i32 wrap_sub(i32 first, i32 second)
{
    return std::bit_cast<i32>(u32(first) - u32(second));
}
i32 truncate(float value)
{
    return _mm_cvttss_si32(_mm_set_ss(value));
}
void put(std::span<u8> bytes, size_t offset, const Vec3& value)
{
    std::memcpy(bytes.data() + offset, &value, 12);
}
}

// Xbox 0x00053720
bool collide_runtime_physical_mesh_segment(RuntimeCollisionShapeCall& call,
    std::span<const u8> shape, const std::function<std::span<const u8>(u32, size_t)>& resolve)
{
    const float* m = call.inverse;
    const Vec3& p = call.candidate;
    const Vec3& q = call.kept;
    const Vec3 begin{((m[1] * p.y + p.x * m[0]) + p.z * m[2]) + m[3],
        ((m[6] * p.z + p.x * m[4]) + m[5] * p.y) + m[7],
        ((m[9] * p.y + p.x * m[8]) + m[10] * p.z) + m[11]};
    const Vec3 finish{((q.x * m[0] + q.z * m[2]) + m[1] * q.y) + m[3],
        ((m[6] * q.z + m[5] * q.y) + m[4] * q.x) + m[7],
        ((q.x * m[8] + m[10] * q.z) + m[9] * q.y) + m[11]};
    Vec3 movement{finish.x - begin.x, finish.y - begin.y, finish.z - begin.z};
    Vec3 normal;
    std::array<i32, 3> lower, upper;
    runtime_mesh_query_cell_bounds(shape, finish, begin, 0.f, lower, upper);
    const Vec3* vertices = reinterpret_cast<const Vec3*>(resolve(call.data_address, 12).data());
    const u8* selected = nullptr;
    u32 selected_address = 0;
    float fraction;
    bool finished = false;
    for (i32 z = lower[2]; z <= upper[2] && !finished; ++z)
    {
        for (i32 y = lower[1]; y <= upper[1] && !finished; ++y)
        {
            for (i32 x = lower[0]; x <= upper[0]; ++x)
            {
                const u32 index = u32(get(shape, model_grid_mesh_field::plane_stride)) * u32(z)
                    + u32(get(shape, model_grid_mesh_field::row_stride)) * u32(y) + u32(x);
                const size_t stride = model_grid_mesh_field::cells.stride;
                const auto cell = shape.subspan(model_grid_mesh_field::cells.offset + size_t(index) * stride, stride);
                const u32 count = get(cell, model_grid_cell_field::triangle_count);
                const u32 address = get(cell, model_grid_cell_field::triangles);
                const auto rows = resolve(address, size_t(count) * model_mesh_triangle_field::kSize);
                const u8* hit = select_runtime_mesh_segment_triangle(rows, vertices, begin, movement,
                    normal, call.mask, u32(call.mode), fraction);
                if (hit != nullptr)
                {
                    selected = hit;
                    selected_address = address + u32(hit - rows.data());
                    if (fraction == 0.f)
                    {
                        finished = true;
                        break;
                    }
                }
            }
        }
    }
    if (selected != nullptr)
    {
        const Vec3 contact{movement.x + begin.x, movement.y + begin.y, movement.z + begin.z};
        const float* w = call.world;
        call.kept = {((w[2] * contact.z + w[1] * contact.y) + w[0] * contact.x) + w[3],
            ((w[6] * contact.z + w[5] * contact.y) + w[4] * contact.x) + w[7],
            ((w[10] * contact.z + w[9] * contact.y) + w[8] * contact.x) + w[11]};
        if (call.record.data() != nullptr)
        {
            write_runtime_mesh_triangle_contact(call.record, {selected, model_mesh_triangle_field::kSize},
                selected_address, vertices, normal, contact, 0.f, w);
        }
        return true;
    }
    return false;
}

// Xbox 0x000553D0
const u8* select_runtime_mesh_swept_triangle(std::span<const u8> triangles,
    const Vec3* vertices, const Vec3& origin, Vec3& movement, float radius,
    Vec3& normal, u32 mask, u32 excluded, float& fraction, const Vec3& stationary)
{
    const u8* selected = nullptr;
    for (size_t offset = 0; offset < triangles.size(); offset += model_mesh_triangle_field::kSize)
    {
        const auto row = triangles.subspan(offset, model_mesh_triangle_field::kSize);
        const u32 flags = get(row, model_mesh_triangle_field::flags);
        const auto corners = get(row, model_mesh_triangle_field::vertices);
        if ((mask & flags) != 0 && ((excluded & 0xFFF3BFFFu) == 0 || (excluded & flags) == 0)
            && ((flags & 0xC4000) == 0 || (~excluded & flags & 0xC4000) != 0))
        {
            if (sweep_runtime_collision_triangle(vertices[corners[0]], vertices[corners[1]], vertices[corners[2]],
                origin, movement, radius, normal, fraction, stationary))
            {
                movement = {movement.x * fraction, movement.y * fraction, movement.z * fraction};
                selected = row.data();
                if (fraction == 0.f)
                {
                    return selected;
                }
            }
        }
    }
    return selected;
}

// Xbox 0x00053AA0
bool collide_runtime_physical_mesh_sweep(RuntimeCollisionShapeCall& call,
    std::span<const u8> shape, const std::function<std::span<const u8>(u32, size_t)>& resolve)
{
    const float* m = call.inverse;
    const Vec3& p = call.candidate;
    const Vec3& q = call.kept;
    const Vec3 begin{((m[1] * p.y + p.z * m[2]) + p.x * m[0]) + m[3],
        ((m[6] * p.z + p.x * m[4]) + m[5] * p.y) + m[7],
        ((m[9] * p.y + p.x * m[8]) + m[10] * p.z) + m[11]};
    const Vec3 finish{((q.x * m[0] + q.z * m[2]) + m[1] * q.y) + m[3],
        ((m[6] * q.z + m[5] * q.y) + q.x * m[4]) + m[7],
        ((q.x * m[8] + m[10] * q.z) + m[9] * q.y) + m[11]};
    const float square = (m[8] * m[8] + m[4] * m[4]) + m[0] * m[0];
    const long double scale = square == 0.f ? 0.L : retail_square_root(square);
    const float radius = float(scale * call.radius);
    Vec3 movement{finish.x - begin.x, finish.y - begin.y, finish.z - begin.z};
    Vec3 normal;
    std::array<i32, 3> lower, upper;
    runtime_mesh_query_cell_bounds(shape, begin, finish, radius, lower, upper);
    const Vec3* vertices = reinterpret_cast<const Vec3*>(resolve(call.data_address, 12).data());
    const u8* selected = nullptr;
    u32 selected_address = 0;
    float fraction;
    bool finished = false;
    for (i32 z = lower[2]; z <= upper[2] && !finished; ++z)
    {
        for (i32 y = lower[1]; y <= upper[1] && !finished; ++y)
        {
            for (i32 x = lower[0]; x <= upper[0]; ++x)
            {
                const u32 index = u32(get(shape, model_grid_mesh_field::plane_stride)) * u32(z)
                    + u32(get(shape, model_grid_mesh_field::row_stride)) * u32(y) + u32(x);
                const size_t stride = model_grid_mesh_field::cells.stride;
                const auto cell = shape.subspan(model_grid_mesh_field::cells.offset + size_t(index) * stride, stride);
                const u32 count = get(cell, model_grid_cell_field::triangle_count);
                const u32 address = get(cell, model_grid_cell_field::triangles);
                const auto rows = resolve(address, size_t(count) * model_mesh_triangle_field::kSize);
                const u8* hit = select_runtime_mesh_swept_triangle(rows, vertices, begin, movement, radius,
                    normal, call.mask, u32(call.mode), fraction, call.stationary);
                if (hit != nullptr)
                {
                    selected = hit;
                    selected_address = address + u32(hit - rows.data());
                    if (fraction == 0.f)
                    {
                        finished = true;
                        break;
                    }
                }
            }
        }
    }
    if (selected != nullptr)
    {
        const Vec3 contact{movement.x + begin.x, movement.y + begin.y, movement.z + begin.z};
        const float* w = call.world;
        call.kept = {((w[2] * contact.z + w[1] * contact.y) + w[0] * contact.x) + w[3],
            ((w[6] * contact.z + w[5] * contact.y) + w[4] * contact.x) + w[7],
            ((w[10] * contact.z + w[9] * contact.y) + w[8] * contact.x) + w[11]};
        if (call.record.data() != nullptr)
        {
            write_runtime_mesh_triangle_contact(call.record, {selected, model_mesh_triangle_field::kSize},
                selected_address, vertices, normal, contact, radius, w);
        }
        return true;
    }
    return false;
}

// Xbox 0x00054DE0
void write_runtime_posed_mesh_contact(std::span<u8> record, std::span<const u8> triangle,
    u32 triangle_address, const Vec3* vertices, const Vec3& normal,
    const Vec3& point, float radius, const float* world, const float* bone)
{
    Vec3 contact = point;
    if (radius != 0.f)
    {
        const float distance = 0.f - radius;
        contact = {point.x + normal.x * distance, point.y + normal.y * distance,
            point.z + normal.z * distance};
    }
    put(record, 0, contact);
    put(record, 0xC, normal);
    if (bone == nullptr)
    {
        record[0x28] = 0;
        record[0x29] = 0x28;
        record[0x2A] = 1;
        put(record, 0x2C, vertices[read_u32(triangle, 0)]);
        put(record, 0x38, vertices[read_u32(triangle, 4)]);
        put(record, 0x44, vertices[read_u32(triangle, 8)]);
    }
    else
    {
        record[0x28] = 0;
        record[0x2A] = 1;
        const Vec3 a = vertices[read_u32(triangle, 0)];
        const Vec3 b = vertices[read_u32(triangle, 4)];
        const Vec3 c = vertices[read_u32(triangle, 8)];
        put(record, 0x2C, {((a.y * bone[1] + a.x * bone[0]) + bone[2] * a.z) + bone[3],
            ((bone[6] * a.z + a.y * bone[5]) + bone[4] * a.x) + bone[7],
            ((bone[10] * a.z + a.x * bone[8]) + a.y * bone[9]) + bone[11]});
        put(record, 0x38, {((b.x * bone[0] + b.y * bone[1]) + b.z * bone[2]) + bone[3],
            ((bone[4] * b.x + b.z * bone[6]) + b.y * bone[5]) + bone[7],
            ((bone[10] * b.z + bone[9] * b.y) + b.x * bone[8]) + bone[11]});
        put(record, 0x44, {((c.y * bone[1] + c.x * bone[0]) + c.z * bone[2]) + bone[3],
            ((c.x * bone[4] + c.z * bone[6]) + c.y * bone[5]) + bone[7],
            ((c.x * bone[8] + bone[10] * c.z) + bone[9] * c.y) + bone[11]});
    }
    record[0x29] = 0x28;
    const Vec3 transformed{((world[2] * contact.z + world[1] * contact.y) + world[0] * contact.x) + world[3],
        ((world[6] * contact.z + world[5] * contact.y) + world[4] * contact.x) + world[7],
        ((world[10] * contact.z + world[9] * contact.y) + world[8] * contact.x) + world[11]};
    put(record, 0, transformed);
    Vec3 transformed_normal;
    transform_runtime_shape_direction(world, normal, transformed_normal);
    put(record, 0xC, transformed_normal);
    transform_runtime_collision_bound(record.subspan(0x28), world);
    const u16 zero = 0;
    std::memcpy(record.data() + 0x1C, &zero, 2);
    record[0x1E] = triangle[0x11];
    record[0x27] = triangle[0x10];
    std::memcpy(record.data() + 0x20, triangle.data() + 0xC, 4);
    const u32 invalid = 0xFFFFFFFFu;
    std::memcpy(record.data() + 0x18, &invalid, 4);
    std::memcpy(record.data() + 0x24, &zero, 2);
    std::memcpy(record.data() + 0x50, &triangle_address, 4);
    record[0x26] = 1;
}

// Xbox 0x000554D0
bool collect_runtime_mesh_triangle_contacts(std::span<const u8> triangles,
    u32 triangle_address, const Vec3* vertices, const float* world, const Vec3& point,
    float radius, u32 mask, u32 excluded, GhoulySpawnPool** pool, const Vec3& stationary)
{
    bool result = false;
    for (size_t offset = 0; offset < triangles.size(); offset += model_mesh_triangle_field::kSize)
    {
        const auto row = triangles.subspan(offset, model_mesh_triangle_field::kSize);
        const u32 flags = get(row, model_mesh_triangle_field::flags);
        const auto corners = get(row, model_mesh_triangle_field::vertices);
        if ((mask & flags) != 0 && ((excluded & 0xFFF3BFFFu) == 0 || (excluded & flags) == 0)
            && ((flags & 0xC4000) == 0 || (~excluded & flags & 0xC4000) != 0))
        {
            if (runtime_sphere_intersects_triangle(point, radius, vertices[corners[0]], vertices[corners[1]],
                vertices[corners[2]]))
            {
                if (pool != nullptr)
                {
                    auto* rows = *pool;
                    if (rows->cursor == rows->limit)
                    {
                        rows = grow_runtime_shape_pool(rows);
                        *pool = rows;
                    }
                    u8* record = rows->cursor;
                    rows->cursor += rows->stride;
                    write_runtime_posed_mesh_contact({record, 0x54}, row, triangle_address + u32(offset), vertices,
                        stationary, point, radius, world, nullptr);
                }
                result = true;
            }
        }
    }
    return result;
}

// Xbox 0x00053F20
bool collect_runtime_physical_mesh_contacts(RuntimeCollisionShapeCall& call,
    std::span<const u8> shape, const std::function<std::span<const u8>(u32, size_t)>& resolve)
{
    const float* m = call.inverse;
    const Vec3& p = call.candidate;
    const Vec3 point{((m[2] * p.z + m[1] * p.y) + p.x * m[0]) + m[3],
        ((m[6] * p.z + m[5] * p.y) + p.x * m[4]) + m[7],
        ((m[10] * p.z + m[9] * p.y) + p.x * m[8]) + m[11]};
    const float square = (m[8] * m[8] + m[0] * m[0]) + m[4] * m[4];
    const long double scale = square == 0.f ? 0.L : retail_square_root(square);
    const float radius = float(scale * call.radius);
    std::array<i32, 3> lower, upper;
    runtime_mesh_query_cell_bounds(shape, point, point, radius, lower, upper);
    const Vec3* vertices = reinterpret_cast<const Vec3*>(resolve(call.data_address, 12).data());
    bool result = false;
    for (i32 z = lower[2]; z <= upper[2]; ++z)
    {
        for (i32 y = lower[1]; y <= upper[1]; ++y)
        {
            for (i32 x = lower[0]; x <= upper[0]; ++x)
            {
                const u32 index = u32(get(shape, model_grid_mesh_field::plane_stride)) * u32(z)
                    + u32(get(shape, model_grid_mesh_field::row_stride)) * u32(y) + u32(x);
                const size_t stride = model_grid_mesh_field::cells.stride;
                const auto cell = shape.subspan(model_grid_mesh_field::cells.offset + size_t(index) * stride, stride);
                const u32 count = get(cell, model_grid_cell_field::triangle_count);
                const u32 address = get(cell, model_grid_cell_field::triangles);
                if (collect_runtime_mesh_triangle_contacts(
                        resolve(address, size_t(count) * model_mesh_triangle_field::kSize), address, vertices,
                        call.world, point, radius, call.mask, u32(call.mode), call.pool, call.stationary))
                {
                    result = true;
                }
            }
        }
    }
    return result;
}

// Xbox 0x00054520
bool collide_runtime_posed_mesh_segment(RuntimeCollisionShapeCall& call, const ModelPoseBuffer* pose,
    std::span<const u8> shape, const std::function<std::span<const u8>(u32, size_t)>& resolve)
{
    float world[12], inverse[12];
    runtime_mesh_pose_matrices(pose, get(shape, model_posed_mesh_field::joint), call.world, call.inverse, world,
        inverse);
    const Vec3& p = call.candidate;
    const Vec3& q = call.kept;
    const float* m = inverse;
    const Vec3 begin{((p.y * m[1] + p.z * m[2]) + p.x * m[0]) + m[3],
        ((p.y * m[5] + p.z * m[6]) + p.x * m[4]) + m[7],
        ((p.y * m[9] + p.z * m[10]) + p.x * m[8]) + m[11]};
    const Vec3 finish{((q.x * m[0] + q.y * m[1]) + q.z * m[2]) + m[3],
        ((q.x * m[4] + q.y * m[5]) + q.z * m[6]) + m[7],
        ((q.x * m[8] + q.y * m[9]) + q.z * m[10]) + m[11]};
    Vec3 movement{finish.x - begin.x, finish.y - begin.y, finish.z - begin.z};
    Vec3 normal;
    float fraction;
    const u32 address = get(shape, model_posed_mesh_field::triangles);
    const auto triangles =
        resolve(address, size_t(get(shape, model_posed_mesh_field::triangle_count)) * model_mesh_triangle_field::kSize);
    const Vec3* vertices = reinterpret_cast<const Vec3*>(resolve(call.data_address, 12).data());
    const u8* hit = select_runtime_mesh_segment_triangle(triangles, vertices, begin, movement, normal,
        call.mask, u32(call.mode), fraction);
    if (hit != nullptr)
    {
        const Vec3 contact{movement.x + begin.x, movement.y + begin.y, movement.z + begin.z};
        call.kept = {((world[2] * contact.z + world[1] * contact.y) + world[0] * contact.x) + world[3],
            ((world[6] * contact.z + world[5] * contact.y) + world[4] * contact.x) + world[7],
            ((world[10] * contact.z + world[9] * contact.y) + world[8] * contact.x) + world[11]};
        if (call.record.data() != nullptr)
        {
            write_runtime_posed_mesh_contact(call.record, {hit, model_mesh_triangle_field::kSize},
                address + u32(hit - triangles.data()), vertices, normal, contact, 0.f, world, nullptr);
        }
        return true;
    }
    return false;
}

// Xbox 0x00054850
bool collide_runtime_posed_mesh_sweep(RuntimeCollisionShapeCall& call, const ModelPoseBuffer* pose,
    std::span<const u8> shape, const std::function<std::span<const u8>(u32, size_t)>& resolve)
{
    float world[12], inverse[12];
    runtime_mesh_pose_matrices(pose, get(shape, model_posed_mesh_field::joint), call.world, call.inverse, world,
        inverse);
    const Vec3& p = call.candidate;
    const Vec3& q = call.kept;
    const float* m = inverse;
    const Vec3 begin{((p.y * m[1] + p.z * m[2]) + p.x * m[0]) + m[3],
        ((p.x * m[4] + p.y * m[5]) + p.z * m[6]) + m[7],
        ((p.x * m[8] + p.y * m[9]) + p.z * m[10]) + m[11]};
    const Vec3 finish{((q.x * m[0] + q.y * m[1]) + q.z * m[2]) + m[3],
        ((q.x * m[4] + q.y * m[5]) + q.z * m[6]) + m[7],
        ((q.x * m[8] + q.y * m[9]) + q.z * m[10]) + m[11]};
    const float square = (m[8] * m[8] + m[4] * m[4]) + m[0] * m[0];
    const long double scale = square == 0.f ? 0.L : retail_square_root(square);
    const float radius = float(scale * call.radius);
    Vec3 movement{finish.x - begin.x, finish.y - begin.y, finish.z - begin.z};
    Vec3 normal;
    float fraction;
    const u32 address = get(shape, model_posed_mesh_field::triangles);
    const auto triangles =
        resolve(address, size_t(get(shape, model_posed_mesh_field::triangle_count)) * model_mesh_triangle_field::kSize);
    const Vec3* vertices = reinterpret_cast<const Vec3*>(resolve(call.data_address, 12).data());
    const u8* hit = select_runtime_mesh_swept_triangle(triangles, vertices, begin, movement, radius,
        normal, call.mask, u32(call.mode), fraction, call.stationary);
    if (hit != nullptr)
    {
        const Vec3 contact{movement.x + begin.x, movement.y + begin.y, movement.z + begin.z};
        call.kept = {((world[2] * contact.z + world[1] * contact.y) + world[0] * contact.x) + world[3],
            ((world[6] * contact.z + world[5] * contact.y) + world[4] * contact.x) + world[7],
            ((world[10] * contact.z + world[9] * contact.y) + world[8] * contact.x) + world[11]};
        if (call.record.data() != nullptr)
        {
            write_runtime_posed_mesh_contact(call.record, {hit, model_mesh_triangle_field::kSize},
                address + u32(hit - triangles.data()), vertices, normal, contact, radius, world, nullptr);
        }
        return true;
    }
    return false;
}

// Xbox 0x00054C40
bool collect_runtime_posed_mesh_contacts(RuntimeCollisionShapeCall& call, const ModelPoseBuffer* pose,
    std::span<const u8> shape, const std::function<std::span<const u8>(u32, size_t)>& resolve)
{
    float world[12], inverse[12];
    runtime_mesh_pose_matrices(pose, get(shape, model_posed_mesh_field::joint), call.world, call.inverse, world,
        inverse);
    const float* m = inverse;
    const Vec3& p = call.candidate;
    const Vec3 point{((p.y * m[1] + p.z * m[2]) + p.x * m[0]) + m[3],
        ((p.y * m[5] + p.z * m[6]) + p.x * m[4]) + m[7],
        ((p.y * m[9] + p.z * m[10]) + p.x * m[8]) + m[11]};
    const float square = (m[8] * m[8] + m[4] * m[4]) + m[0] * m[0];
    const float scale = square != 0.f ? float(retail_square_root(square)) : 0.f;
    const u32 address = get(shape, model_posed_mesh_field::triangles);
    const auto triangles =
        resolve(address, size_t(get(shape, model_posed_mesh_field::triangle_count)) * model_mesh_triangle_field::kSize);
    const Vec3* vertices = reinterpret_cast<const Vec3*>(resolve(call.data_address, 12).data());
    return collect_runtime_mesh_triangle_contacts(triangles, address, vertices, world, point,
        scale * call.radius, call.mask, u32(call.mode), call.pool, call.stationary);
}

// Xbox 0x00054340
void runtime_mesh_query_cell_bounds(std::span<const u8> shape, const Vec3& first,
    const Vec3& second, float radius, std::array<i32, 3>& lower, std::array<i32, 3>& upper)
{
    const float start[3]{first.x, first.y, first.z};
    const float end[3]{second.x, second.y, second.z};
    const i32 margin = truncate(radius + 1.f);
    for (size_t axis = 0; axis < 3; ++axis)
    {
        i32 low, high;
        if (start[axis] > end[axis])
        {
            low = truncate(end[axis]);
            high = truncate(start[axis]);
        }
        else
        {
            low = truncate(start[axis]);
            high = truncate(end[axis]);
        }
        const i32 origin = std::bit_cast<i16>(get(shape, model_grid_mesh_field::origin)[axis]);
        low = wrap_sub(wrap_sub(low, origin), margin);
        high = wrap_add(high, wrap_sub(margin, origin));
        const i32 step = get(shape, model_grid_mesh_field::cell_size);
        const i32 low_cell = low < 0 ? wrap_sub(low / step, 1) : low / step;
        const i32 high_cell = high < 0 ? wrap_sub(high / step, 1) : high / step;
        const i32 minimum = std::bit_cast<i16>(get(shape, model_grid_mesh_field::cell_min)[axis]);
        const i32 maximum = std::bit_cast<i16>(get(shape, model_grid_mesh_field::cell_max)[axis]);
        lower[axis] = low_cell;
        upper[axis] = high_cell;
        if (lower[axis] < minimum)
        {
            lower[axis] = minimum;
        }
        else if (maximum < lower[axis])
        {
            lower[axis] = maximum;
        }
        if (upper[axis] < minimum)
        {
            upper[axis] = minimum;
        }
        else if (maximum < upper[axis])
        {
            upper[axis] = maximum;
        }
        lower[axis] = wrap_sub(lower[axis], minimum);
        upper[axis] = wrap_sub(upper[axis], minimum);
    }
}

// Xbox 0x00103400
bool intersect_runtime_mesh_triangle(const Vec3& direction, const Vec3& second,
    const Vec3& third, const Vec3& first, const Vec3& origin, float& fraction)
{
    const Vec3 edge{third.x - first.x, third.y - first.y, third.z - first.z};
    const Vec3 other{second.x - first.x, second.y - first.y, second.z - first.z};
    const Vec3 offset{origin.x - first.x, origin.y - first.y, origin.z - first.z};
    const Vec3 cross{direction.y * edge.z - direction.z * edge.y,
        direction.z * edge.x - direction.x * edge.z,
        direction.x * edge.y - direction.y * edge.x};
    const float determinant = (cross.y * other.y + cross.z * other.z) + other.x * cross.x;
    if (std::bit_cast<float>(0x3456BF95u) > determinant)
    {
        return false;
    }
    const float first_weight = (offset.z * cross.z + offset.y * cross.y) + offset.x * cross.x;
    if (0.f > first_weight || first_weight > determinant)
    {
        return false;
    }
    const Vec3 second_cross{offset.y * other.z - offset.z * other.y,
        offset.z * other.x - offset.x * other.z, offset.x * other.y - offset.y * other.x};
    const float second_weight = (direction.y * second_cross.y + direction.z * second_cross.z)
        + direction.x * second_cross.x;
    if (0.f > second_weight || second_weight + first_weight > determinant)
    {
        return false;
    }
    const float selected = ((second_cross.z * edge.z + second_cross.y * edge.y) + edge.x * second_cross.x) / determinant;
    fraction = selected;
    if (0.f > selected || selected > 1.f)
    {
        return false;
    }
    return true;
}

// Xbox 0x000551A0
const u8* select_runtime_mesh_segment_triangle(std::span<const u8> triangles,
    const Vec3* vertices, const Vec3& origin, Vec3& movement, Vec3& normal,
    u32 mask, u32 excluded, float& fraction)
{
    const u8* selected = nullptr;
    for (size_t offset = 0; offset < triangles.size(); offset += model_mesh_triangle_field::kSize)
    {
        const auto row = triangles.subspan(offset, model_mesh_triangle_field::kSize);
        const u32 flags = get(row, model_mesh_triangle_field::flags);
        const auto corners = get(row, model_mesh_triangle_field::vertices);
        if ((mask & flags) != 0 && ((excluded & 0xFFF3BFFFu) == 0 || (excluded & flags) == 0)
            && ((flags & 0xC4000) == 0 || (~excluded & flags & 0xC4000) != 0))
        {
            const Vec3& first = vertices[corners[0]];
            const Vec3& second = vertices[corners[1]];
            const Vec3& third = vertices[corners[2]];
            if (intersect_runtime_mesh_triangle(movement, second, third, first, origin, fraction))
            {
                movement = {movement.x * fraction, movement.y * fraction, movement.z * fraction};
                const Vec3 a{second.x - first.x, second.y - first.y, second.z - first.z};
                const Vec3 b{third.x - first.x, third.y - first.y, third.z - first.z};
                normal = {b.z * a.y - b.y * a.z, b.x * a.z - b.z * a.x, b.y * a.x - a.y * b.x};
                const float square = (normal.x * normal.x + normal.y * normal.y) + normal.z * normal.z;
                if (square != 0.f)
                {
                    const long double reciprocal = retail_reciprocal_sqrt(square);
                    normal = {float(reciprocal * normal.x), float(reciprocal * normal.y), float(reciprocal * normal.z)};
                }
                selected = row.data();
                if (fraction == 0.f)
                {
                    return selected;
                }
            }
        }
    }
    return selected;
}

// Xbox 0x00054170
void write_runtime_mesh_triangle_contact(std::span<u8> record, std::span<const u8> triangle,
    u32 triangle_address, const Vec3* vertices, const Vec3& normal, const Vec3& point,
    float radius, const float* world)
{
    Vec3 contact = point;
    if (radius != 0.f)
    {
        const float distance = 0.f - radius;
        contact = {point.x + normal.x * distance, point.y + normal.y * distance,
            point.z + normal.z * distance};
    }
    put(record, 0, contact);
    put(record, 0xC, normal);
    record[0x29] = 0x28;
    record[0x28] = 0;
    record[0x2A] = 1;
    put(record, 0x2C, vertices[read_u32(triangle, 0)]);
    put(record, 0x38, vertices[read_u32(triangle, 4)]);
    put(record, 0x44, vertices[read_u32(triangle, 8)]);
    record[0x29] = 0x28;
    const Vec3 transformed{((world[2] * contact.z + world[1] * contact.y) + world[0] * contact.x) + world[3],
        ((world[6] * contact.z + world[5] * contact.y) + world[4] * contact.x) + world[7],
        ((world[10] * contact.z + world[9] * contact.y) + world[8] * contact.x) + world[11]};
    put(record, 0, transformed);
    Vec3 transformed_normal;
    transform_runtime_shape_direction(world, normal, transformed_normal);
    put(record, 0xC, transformed_normal);
    transform_runtime_collision_bound(record.subspan(0x28), world);
    const u16 zero = 0;
    std::memcpy(record.data() + 0x1C, &zero, 2);
    record[0x1E] = triangle[0x11];
    record[0x27] = triangle[0x10];
    std::memcpy(record.data() + 0x20, triangle.data() + 0xC, 4);
    const u32 invalid = 0xFFFFFFFFu;
    std::memcpy(record.data() + 0x18, &invalid, 4);
    std::memcpy(record.data() + 0x24, &zero, 2);
    std::memcpy(record.data() + 0x50, &triangle_address, 4);
    record[0x26] = 1;
}
}
