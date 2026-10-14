#include "body_shape_push.h"
#include "actor_transform.h"
#include "gbtg/model_runtime.h"
#include <bit>
#include <cmath>
#include <cstring>

namespace gbtg
{
// Xbox 0x001202C0
void merge_runtime_collision_cylinders(const Vec3& point, EntityCylinderBound* shapes,
    i32 count, EntityCylinderBound& output)
{
    float top = point.y;
    float bottom = top;
    float radius = 0.f;
    for (i32 index = 0; index < count; ++index)
    {
        const auto& shape = shapes[index];
        const float dz = shape.z - point.z;
        const float dx = shape.x - point.x;
        const float reach = float(retail_square_root(dz * dz + dx * dx) + shape.radius);
        if (reach > radius)
        {
            radius = reach;
        }
        if (!(shape.y > bottom))
        {
            bottom = shape.y;
        }
        const float high = shape.height + shape.y;
        if (!(top >= high))
        {
            top = high;
        }
    }
    output.kind = 4;
    output.size = 0x20;
    output.flag = 1;
    output.x = point.x;
    output.y = bottom;
    output.z = point.z;
    output.height = top - bottom;
    output.pad0 = 0.f;
    output.pad1 = 0.f;
    output.radius = radius;
}

// Xbox 0x00120D10
void shift_runtime_collision_cylinders(const Vec3& push, EntityCylinderBound* shapes,
    i32 count, EntityCylinderBound& merged)
{
    for (i32 index = 0; index < count; ++index)
    {
        shapes[index].x = shapes[index].x + push.x;
        shapes[index].y = push.y + shapes[index].y;
        shapes[index].z = push.z + shapes[index].z;
    }
    merged.x = merged.x + push.x;
    merged.y = push.y + merged.y;
    merged.z = push.z + merged.z;
}

// Xbox 0x00120480
i32 sum_runtime_collision_push(const Vec3& point, const Actor* actor,
    EntityCylinderBound* shapes, i32 count, Vec3& output, RuntimeBodyShapePush& context)
{
    context.contact_count = 0;
    EntityCylinderBound merged;
    merge_runtime_collision_cylinders(point, shapes, count, merged);
    Vec3 query;
    const Vec3* query_pointer = nullptr;
    Vec3 delta;
    if (actor != nullptr)
    {
        delta = actor_world_position(*actor);
        query = Vec3{point.x - delta.x, point.y - delta.y, point.z - delta.z};
        query_pointer = &query;
    }
    output = Vec3{0.f, 0.f, 0.f};
    i32 hit = 0;
    for (auto* probe = context.first; probe != nullptr; probe = probe->next)
    {
        if (probe->push == nullptr)
        {
            throw Error("body shape push reached an unbound actor vtable +0x34");
        }
        if (probe->push(*probe, actor, query_pointer, merged, shapes, count, delta) != 0)
        {
            context.contacts[context.contact_count] = probe;
            ++context.contact_count;
            shift_runtime_collision_cylinders(delta, shapes, count, merged);
            output.x = output.x + delta.x;
            output.y = output.y + delta.y;
            output.z = delta.z + output.z;
            hit = 1;
        }
    }
    return hit;
}

// Xbox 0x001203E0
void push_runtime_collision_point(const Vec3& source, Vec3& target, const Actor* actor,
    float height, float radius, RuntimeBodyShapePush& context)
{
    EntityCylinderBound shape;
    shape.kind = 4;
    shape.size = 0x20;
    shape.flag = 1;
    shape.x = source.x;
    shape.y = source.y;
    shape.z = source.z;
    shape.pad0 = 0.f;
    shape.pad1 = 0.f;
    shape.height = height;
    shape.radius = radius;
    Vec3 push;
    sum_runtime_collision_push(source, actor, &shape, 1, push, context);
    target.x = source.x + push.x;
    target.y = source.y + push.y;
    target.z = push.z + source.z;
}

// Xbox 0x00120250
void scale_runtime_collision_vector(Vec3& vector, float length)
{
    const float squared = (vector.x * vector.x + vector.y * vector.y) + vector.z * vector.z;
    if (squared != 0.f)
    {
        const long double scale = retail_reciprocal_sqrt(squared) * static_cast<long double>(length);
        vector.x = float(scale * static_cast<long double>(vector.x));
        vector.y = float(scale * static_cast<long double>(vector.y));
        vector.z = float(scale * static_cast<long double>(vector.z));
    }
}

// Xbox 0x00120760
bool push_runtime_collision_cylinder(const EntityCylinderBound& moving,
    const EntityCylinderBound& fixed, const Vec3& offset, Vec3& push, Vec3& contact)
{
    const float y = moving.y + offset.y;
    const float x = offset.x + moving.x;
    const float z = moving.z + offset.z;
    if (y > fixed.y + fixed.height || fixed.y > moving.height + y)
    {
        return false;
    }
    const float radius = fixed.radius + moving.radius;
    Vec3 normal{x - fixed.x, 0.f, z - fixed.z};
    const long double metric = retail_square_root(normal.z * normal.z + normal.x * normal.x);
    float distance = float(metric);
    if (metric < static_cast<long double>(std::bit_cast<float>(0x3727C5ACu)))
    {
        normal.x = 1.f;
        distance = radius - std::bit_cast<float>(0x3C23D70Au);
    }
    if (!(radius > distance))
    {
        return false;
    }
    scale_runtime_collision_vector(normal, radius);
    push.x = (fixed.x + normal.x) - x;
    push.z = (fixed.z + normal.z) - z;
    push.y = 0.f;
    const float fixed_mid = fixed.height * 0.5f + fixed.y; // DAT_0032B0F4.
    contact.x = ((offset.x + moving.x) - fixed.x) * 0.5f + fixed.x;
    contact.y = ((offset.y + (moving.height * 0.5f + moving.y)) - fixed_mid) * 0.5f + fixed_mid;
    contact.z = ((offset.z + moving.z) - fixed.z) * 0.5f + fixed.z;
    return true;
}

// Xbox 0x0010ACD0
bool separate_runtime_collision_triangle(std::span<const u8> shape, Vec3& point, float radius)
{
    const float ay = read_f32(shape, 0x20) - read_f32(shape, 8);
    const float az = read_f32(shape, 0x24) - read_f32(shape, 0xC);
    const float by = read_f32(shape, 0x14) - read_f32(shape, 8);
    const float bz = read_f32(shape, 0x18) - read_f32(shape, 0xC);
    const float bx = read_f32(shape, 0x10) - read_f32(shape, 4);
    const float ax = read_f32(shape, 0x1C) - read_f32(shape, 4);
    float ny = bz * ax - az * bx;
    float nz = ay * bx - by * ax;
    float nx = az * by - ay * bz;
    const float squared = (ny * ny + nz * nz) + nx * nx;
    if (squared != 0.f)
    {
        const long double inverse = retail_reciprocal_sqrt(squared);
        nx = float(static_cast<long double>(nx) * inverse);
        ny = float(static_cast<long double>(ny) * inverse);
        nz = float(static_cast<long double>(nz) * inverse);
    }
    const float distance = (ny * (point.y - read_f32(shape, 8))
        + nz * (point.z - read_f32(shape, 0xC))) + nx * (point.x - read_f32(shape, 4));
    if (distance > radius)
    {
        return false;
    }
    const float push = radius - distance;
    constexpr float padding = std::bit_cast<float>(0x3A83126Fu);
    point.y = (ny * push + ny * padding) + point.y;
    point.x = (nx * push + nx * padding) + point.x;
    point.z = (nz * push + nz * padding) + point.z;
    return true;
}

// Xbox 0x0010ABD0
bool separate_runtime_collision_sphere(std::span<const u8> shape, Vec3& point, float radius)
{
    float dx = point.x - read_f32(shape, 4);
    float dy = point.y - read_f32(shape, 8);
    const long double extended_z = static_cast<long double>(point.z) - read_f32(shape, 0xC);
    float dz = float(extended_z);
    radius = read_f32(shape, 0x10) + radius;
    // FST writes dz to float while retaining the unrounded subtraction in ST0.
    const float length = float(std::sqrt((extended_z * dz
        + static_cast<long double>(dx) * dx) + static_cast<long double>(dy) * dy));
    if (length > radius)
    {
        return false;
    }
    const float inverse = 1.f / length;
    dx = dx * inverse;
    dy = dy * inverse;
    dz = dz * inverse;
    point.x = read_f32(shape, 4) + dx * radius;
    point.y = read_f32(shape, 8) + dy * radius;
    point.z = read_f32(shape, 0xC) + dz * radius;
    constexpr float padding = std::bit_cast<float>(0x3A83126Fu);
    point.x = dx * padding + point.x;
    point.y = dy * padding + point.y;
    point.z = dz * padding + point.z;
    return true;
}

// Xbox 0x00056720
void transform_runtime_shape_sphere(const EntityShapeNode& node, const float* world,
    const EntityBonePose* pose, EntityCylinderBound& output)
{
    Vec3 center;
    const float radius = shape_sphere_metrics(node, world, pose, center);
    output.kind = 4;
    output.size = 0x20;
    output.flag = 1;
    output.x = center.x;
    output.y = center.y - radius;
    output.z = center.z;
    output.height = radius + radius;
    output.pad0 = 0.f;
    output.pad1 = 0.f;
    output.radius = radius;
}

// Xbox 0x000756C0
void push_runtime_player_body_shapes(std::span<u8> body, Actor& actor, Vec3& position,
    RuntimeBodyShapeStorage& shapes, RuntimeBodyShapePush& context)
{
    const u8 mode = body[0x244];
    Vec3 push;
    if (mode == 0 || (mode > 1 && mode < 4))
    {
        push = actor_transform_origin;
    }
    else if (shapes.selected_state == nullptr)
    {
        const Vec3 previous = position;
        push_runtime_collision_point(position, position, &actor,
            actor.radius_y * read_f32(body, 0x884),
            actor.radius_xz * read_f32(body, 0x888), context);
        push = Vec3{position.x - previous.x, position.y - previous.y, position.z - previous.z};
    }
    else
    {
        float world[12];
        actor_build_world_matrix(actor, world, &position);
        const EntityBonePose* pose = nullptr;
        if (actor.model != nullptr)
        {
            pose = actor.model->alternate_shape_pose;
            if (pose == nullptr)
            {
                pose = actor.model->primary_shape_pose;
            }
        }
        i32 count = 0;
        for (i32 index = 0; index < i32(read_u32(body, 0x898)); ++index)
        {
            const u8* shape = shapes.selected[index];
            const u32 flags = actor.model == nullptr ? 0xFFFFFFFFu
                : actor.model->runtime->shader_indices[std::bit_cast<i16>(read_u16({shape, 4}, 2))];
            if ((read_u32({shape, 8}, 4) & flags & 4) != 0)
            {
                u8* selected = shapes.selected_state + size_t(count) * 0x20;
                const auto& node = *reinterpret_cast<const EntityShapeNode*>(shape);
                if (shape[0] == 0)
                {
                    transform_runtime_shape_sphere(node, world, pose,
                        *reinterpret_cast<EntityCylinderBound*>(selected));
                }
                else if (shape[0] == 1)
                {
                    transform_runtime_shape_cylinder(node, world, pose, selected);
                }
                else
                {
                    continue;
                }
                ++count;
            }
        }
        if (count > 0)
        {
            sum_runtime_collision_push(position, &actor,
                reinterpret_cast<EntityCylinderBound*>(shapes.selected_state), count, push, context);
            position.x = position.x + push.x;
            position.y = push.y + position.y;
            position.z = position.z + push.z;
        }
        else
        {
            push = actor_transform_origin;
        }
    }
    std::memcpy(body.data() + 0x248, &push.x, 4);
    std::memcpy(body.data() + 0x24C, &push.y, 4);
    std::memcpy(body.data() + 0x250, &push.z, 4);
}
}
