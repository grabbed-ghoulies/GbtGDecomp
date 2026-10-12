#include "model_sphere_collision.h"
#include "actor_collision_culling.h"
#include "contact_sweep.h"
#include "gbtg/anim.h"
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
Vec3 point(std::span<const u8> bytes, size_t offset)
{
    return {read_f32(bytes, offset), read_f32(bytes, offset + 4), read_f32(bytes, offset + 8)};
}
void put_point(std::span<u8> bytes, size_t offset, const Vec3& value)
{
    std::memcpy(bytes.data() + offset, &value, 12);
}
Vec3 world_point(const float* m, const Vec3& p)
{
    return {((m[2] * p.z + m[1] * p.y) + m[0] * p.x) + m[3],
        ((m[6] * p.z + m[4] * p.x) + m[5] * p.y) + m[7],
        ((m[10] * p.z + m[8] * p.x) + m[9] * p.y) + m[11]};
}
Vec3 hit_world_point(const float* m, const Vec3& p)
{
    return {((m[2] * p.z + m[1] * p.y) + m[0] * p.x) + m[3],
        ((m[6] * p.z + m[5] * p.y) + m[4] * p.x) + m[7],
        ((m[10] * p.z + m[9] * p.y) + m[8] * p.x) + m[11]};
}
float matrix_scale(const float* m)
{
    const float square = (m[8] * m[8] + m[4] * m[4]) + m[0] * m[0];
    return square != 0.f ? float(retail_square_root(square)) : 0.f;
}
}

// Xbox 0x0010AAE0
void transform_runtime_collision_sphere(std::span<u8> bound, const float* matrix)
{
    const Vec3 p = point(bound, 4);
    const Vec3 transformed{((matrix[2] * p.z + matrix[1] * p.y) + p.x * matrix[0]) + matrix[3],
        ((matrix[6] * p.z + matrix[5] * p.y) + matrix[4] * p.x) + matrix[7],
        ((matrix[10] * p.z + matrix[9] * p.y) + p.x * matrix[8]) + matrix[11]};
    put_point(bound, 4, transformed);
    const float radius = matrix_scale(matrix) * read_f32(bound, 0x10);
    std::memcpy(bound.data() + 0x10, &radius, 4);
}

// Xbox 0x0010AF90
void transform_runtime_collision_bound(std::span<u8> bound, const float* matrix)
{
    if (bound[0] == 0)
    {
        for (size_t offset : {size_t(4), size_t(0x10), size_t(0x1C)})
        {
            const Vec3 p = point(bound, offset);
            const Vec3 transformed{((p.y * matrix[1] + matrix[2] * p.z) + p.x * matrix[0]) + matrix[3],
                ((p.x * matrix[4] + p.z * matrix[6]) + p.y * matrix[5]) + matrix[7],
                ((p.x * matrix[8] + p.z * matrix[10]) + p.y * matrix[9]) + matrix[11]};
            put_point(bound, offset, transformed);
        }
    }
    else if (bound[0] == 1)
    {
        transform_runtime_collision_sphere(bound, matrix);
    }
}

// Xbox 0x000563F0
void write_runtime_sphere_contact(std::span<u8> record, std::span<const u8> shape,
    u32 shape_address, const Vec3& contact, const Vec3& center, float radius, const float* world)
{
    put_point(record, 0, contact);
    const float reciprocal = 1.f / radius;
    const Vec3 normal{reciprocal * (contact.x - center.x), (contact.y - center.y) * reciprocal,
        (contact.z - center.z) * reciprocal};
    put_point(record, 0xC, normal);
    record[0x29] = 0x14;
    record[0x28] = 1;
    record[0x2A] = 1;
    put_point(record, 0x2C, center);
    std::memcpy(record.data() + 0x38, &radius, 4);
    record[0x29] = 0x28;
    put_point(record, 0, world_point(world, contact));
    Vec3 transformed_normal;
    transform_runtime_shape_direction(world, normal, transformed_normal);
    put_point(record, 0xC, transformed_normal);
    transform_runtime_collision_bound(record.subspan(0x28), world);
    std::memcpy(record.data() + 0x1C, shape.data() + 0xA, 2);
    record[0x1E] = shape[0xC];
    std::memcpy(record.data() + 0x20, shape.data() + 4, 4);
    record[0x27] = 0;
    const u32 value = shape[0] == 3 ? read_u32(shape, 0x20) : 0xFFFFFFFFu;
    std::memcpy(record.data() + 0x18, &value, 4);
    std::memcpy(record.data() + 0x24, shape.data() + 2, 2);
    std::memcpy(record.data() + 0x50, &shape_address, 4);
    record[0x26] = 1;
}

// Xbox 0x00055CB0
bool collide_runtime_model_sphere_segment(const EntityShapeNode& shape, u32 shape_address,
    const float* world, const float* inverse, const EntityBonePose* pose,
    const Vec3& start, Vec3& end, std::span<u8> record, const Vec3& stationary)
{
    const Vec3 begin{((inverse[1] * start.y + inverse[0] * start.x) + start.z * inverse[2]) + inverse[3],
        ((inverse[6] * start.z + inverse[5] * start.y) + inverse[4] * start.x) + inverse[7],
        ((inverse[9] * start.y + start.x * inverse[8]) + inverse[10] * start.z) + inverse[11]};
    const Vec3 finish{((end.z * inverse[2] + inverse[1] * end.y) + inverse[0] * end.x) + inverse[3],
        ((inverse[6] * end.z + inverse[5] * end.y) + inverse[4] * end.x) + inverse[7],
        ((end.x * inverse[8] + inverse[10] * end.z) + inverse[9] * end.y) + inverse[11]};
    Vec3 center;
    const float radius = shape_sphere_metrics(shape, nullptr, pose, center);
    if (radius == 0.f)
    {
        return false;
    }
    const Vec3 movement{finish.x - begin.x, finish.y - begin.y, finish.z - begin.z};
    float fraction;
    if (sweep_runtime_collision_spheres(begin, movement, center, radius, stationary, 1.f, fraction) && fraction > 0.f)
    {
        const Vec3 contact{movement.x * fraction + begin.x, movement.y * fraction + begin.y,
            movement.z * fraction + begin.z};
        end = hit_world_point(world, contact);
        if (record.data() != nullptr)
        {
            write_runtime_sphere_contact(record, {reinterpret_cast<const u8*>(&shape), 0x24}, shape_address,
                contact, center, radius, world);
        }
        return true;
    }
    return false;
}

// Xbox 0x00055F80
bool collide_runtime_model_sphere_sweep(const EntityShapeNode& shape, u32 shape_address,
    const float* world, const float* inverse, const EntityBonePose* pose,
    const Vec3& start, Vec3& end, float query_radius, std::span<u8> record, const Vec3& stationary)
{
    const Vec3 begin{((inverse[1] * start.y + inverse[0] * start.x) + start.z * inverse[2]) + inverse[3],
        ((inverse[6] * start.z + start.x * inverse[4]) + inverse[5] * start.y) + inverse[7],
        ((inverse[9] * start.y + start.x * inverse[8]) + inverse[10] * start.z) + inverse[11]};
    const Vec3 finish{((inverse[0] * end.x + end.z * inverse[2]) + inverse[1] * end.y) + inverse[3],
        ((inverse[6] * end.z + inverse[5] * end.y) + inverse[4] * end.x) + inverse[7],
        ((end.x * inverse[8] + inverse[10] * end.z) + inverse[9] * end.y) + inverse[11]};
    const float scaled_radius = matrix_scale(inverse) * query_radius;
    Vec3 center;
    const float radius = shape_sphere_metrics(shape, nullptr, pose, center);
    if (radius == 0.f)
    {
        return false;
    }
    const Vec3 movement{finish.x - begin.x, finish.y - begin.y, finish.z - begin.z};
    float fraction;
    if (sweep_runtime_collision_sphere_pair(begin, scaled_radius, movement, center, radius,
        stationary, 1.f, fraction) && fraction > 0.f)
    {
        Vec3 contact{movement.x * fraction + begin.x, movement.y * fraction + begin.y,
            movement.z * fraction + begin.z};
        end = hit_world_point(world, contact);
        if (record.data() != nullptr)
        {
            const Vec3 delta{center.x - contact.x, center.y - contact.y, center.z - contact.z};
            const float scale = scaled_radius / (radius + scaled_radius);
            contact = {delta.x * scale + contact.x, delta.y * scale + contact.y, delta.z * scale + contact.z};
            write_runtime_sphere_contact(record, {reinterpret_cast<const u8*>(&shape), 0x24}, shape_address,
                contact, center, radius, world);
        }
        return true;
    }
    return false;
}

// Xbox 0x00056580
bool collect_runtime_model_sphere_contact(const EntityShapeNode& shape, u32 shape_address,
    const float* world, const float* inverse, const EntityBonePose* pose,
    const Vec3& query, float query_radius, GhoulySpawnPool** pool)
{
    const Vec3 local{((inverse[2] * query.z + inverse[1] * query.y) + inverse[0] * query.x) + inverse[3],
        ((inverse[6] * query.z + inverse[5] * query.y) + inverse[4] * query.x) + inverse[7],
        ((inverse[10] * query.z + inverse[9] * query.y) + query.x * inverse[8]) + inverse[11]};
    const float square = (inverse[8] * inverse[8] + inverse[0] * inverse[0]) + inverse[4] * inverse[4];
    const float scale = square != 0.f ? float(retail_square_root(square)) : 0.f;
    Vec3 center;
    const float radius = shape_sphere_metrics(shape, nullptr, pose, center);
    if (radius == 0.f)
    {
        return false;
    }
    if (runtime_collision_spheres_overlap(local, scale * query_radius, center, radius))
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
            write_runtime_sphere_contact({record, 0x54}, {reinterpret_cast<const u8*>(&shape), 0x24},
                shape_address, local, center, radius, world);
        }
        return true;
    }
    return false;
}

// Xbox 0x00055A90
u32 reject_runtime_model_shape_collision()
{
    return 0;
}
}
