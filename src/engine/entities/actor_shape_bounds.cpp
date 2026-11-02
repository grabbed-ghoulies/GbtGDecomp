#include "actor_shape_bounds.h"
#include "actor_scare_requests.h"
#include "actor_transform.h"
#include "entity_heap.h"
#include "shape_copy.h"
#include "gbtg/anim.h"
#include "body_fields.h"
#include <cmath>
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
const u8* actor_shape_block(const Actor& actor, u32 kind)
{
    if (actor.model != nullptr && actor.model->cpu.data() != nullptr)
    {
        const auto cpu = actor.model->cpu;
        const u32 count = read_u32(cpu, 4);
        const u32 table = read_u32(cpu, 0);
        for (u32 index = 0; index < count; ++index)
        {
            if (read_u32(cpu, table + index * 8) == kind)
            {
                const u32 offset = read_u32(cpu, table + index * 8 + 4);
                return offset != 0 ? cpu.data() + offset : nullptr;
            }
        }
    }
    return nullptr;
}
void bound_store(u8* bytes, size_t offset, float value)
{
    std::memcpy(bytes + offset, &value, 4);
}
float bound_float(const u8* bytes, size_t offset)
{
    float value;
    std::memcpy(&value, bytes + offset, 4);
    return value;
}
void bound_header(u8* bytes)
{
    bytes[0] = 4;
    bytes[1] = 0x20;
    bytes[2] = 1;
}
}

// Xbox 0x00042500
u32 runtime_actor_physical_shape_flags(const Actor& actor)
{
    const u8* block = actor_shape_block(actor, 4);
    if (block == nullptr)
    {
        return 0;
    }
    i32 count = std::bit_cast<i32>(read_u32({block, 4}, 0));
    const u8* row = block + 8;
    u32 flags = 0;
    while (count > 0)
    {
        flags |= read_u32({row, 8}, 4);
        row = row[1] == 0 ? nullptr : row + row[1];
        --count;
    }
    return flags;
}

// Xbox 0x00066260
GhoulySpawnPool* allocate_runtime_shape_pool(u16 stride, u16 count)
{
    const u32 bytes = u32(stride) * count;
    auto* pool = static_cast<GhoulySpawnPool*>(engine_heap_allocate(i32(sizeof(GhoulySpawnPool) + bytes)));
    pool->stride = stride;
    pool->grow = count != 0 ? count : 10;
    pool->begin = reinterpret_cast<u8*>(pool + 1);
    pool->cursor = pool->begin;
    pool->limit = pool->begin + bytes;
    return pool;
}

// Xbox 0x000662A0
GhoulySpawnPool* grow_runtime_shape_pool(GhoulySpawnPool* pool)
{
    const i32 used = i32(pool->cursor - pool->begin) / pool->stride;
    const i32 count = used + pool->grow;
    pool = static_cast<GhoulySpawnPool*>(engine_heap_reallocate(pool,
        i32(sizeof(GhoulySpawnPool)) + i32(pool->stride) * count));
    pool->begin = reinterpret_cast<u8*>(pool + 1);
    pool->cursor = pool->begin + pool->stride * used;
    pool->limit = pool->begin + pool->stride * count;
    return pool;
}

// Xbox 0x00042640
u32 count_runtime_actor_bound_shapes(const Actor& actor, u32 mask)
{
    u32 count = 0;
    const u8* block = actor_shape_block(actor, 4);
    if (block != nullptr)
    {
        const u8* node = block + 8;
        while (node != nullptr)
        {
            if ((read_u32({node, 8}, 4) & mask) != 0)
            {
                ++count;
            }
            if (node[1] == 0)
            {
                return count;
            }
            node += node[1];
        }
    }
    return count;
}

// Xbox 0x0003D9A0
long double actor_model_sweep_radius(const Actor& actor)
{
    const u8* block = actor_shape_block(actor, 5);
    if (block != nullptr)
    {
        const float radius = float(static_cast<long double>(actor.scale_200)
            * bound_float(block, 0x10) * actor.radius_xz);
        return animation_absolute(radius);
    }
    return 0.L; // DAT_0032B100.
}

// Xbox 0x0003D900
void actor_model_bound_center(const Actor& actor, Vec3& output)
{
    const u8* block = actor_shape_block(actor, 5);
    if (block != nullptr)
    {
        output = Vec3{bound_float(block, 4), bound_float(block, 8), bound_float(block, 0xC)};
        const float scale = actor.scale_200 * actor.radius_xz;
        output.x = output.x * scale;
        output.y = output.y * scale;
        output.z = output.z * scale;
        return;
    }
    output.z = 0.f;
    output.y = 0.f;
    output.x = 0.f;
}

// Xbox 0x0003DA00
bool actor_model_world_bound(const Actor& actor, Vec3& center, float& radius)
{
    if (actor_shape_block(actor, 5) == nullptr)
    {
        return false;
    }
    Vec3 local;
    actor_model_bound_center(actor, local);
    const float* m = actor.xform;
    center.x = m[1] * local.y + m[2] * local.z + m[0] * local.x + m[3];
    center.y = m[5] * local.y + m[6] * local.z + m[4] * local.x + m[7];
    center.z = m[9] * local.y + m[10] * local.z + m[8] * local.x + m[11];
    radius = float(actor_model_sweep_radius(actor));
    return true;
}

// Xbox 0x0003DB30
long double actor_model_bound_radius(const Actor& actor)
{
    const u8* block = actor_shape_block(actor, 5);
    if (block != nullptr)
    {
        const float radius = float(static_cast<long double>(actor.scale_200)
            * bound_float(block, 0x24) * actor.radius_xz);
        return animation_absolute(radius);
    }
    return 0.L;
}

// Xbox 0x00103BE0
void transform_runtime_shape_direction(const float* matrix, const Vec3& direction, Vec3& output)
{
    float normalized[12];
    std::memcpy(normalized, matrix, 48);
    normalize_model_matrix(normalized, 1.f);
    const Vec3 point = direction;
    output.x = point.x * normalized[0] + normalized[1] * point.y + normalized[2] * point.z;
    output.y = point.x * normalized[4] + normalized[5] * point.y + normalized[6] * point.z;
    output.z = point.x * normalized[8] + normalized[9] * point.y + normalized[10] * point.z;
}

// Xbox 0x00055B30
void measure_runtime_shape_cylinder(const EntityShapeNode& node, const float* world,
    const EntityBonePose* pose, Vec3& center, Vec3& direction, float& radius,
    float& up, float& down)
{
    shape_transform_point(node, world, pose, node.point, center);
    Vec3 local_direction;
    std::memcpy(&local_direction, reinterpret_cast<const u8*>(&node) + 0x24, 12);
    transform_runtime_shape_direction(world, local_direction, direction);
    Vec3 local = node.point;
    local.x = node.along_x + local.x;
    Vec3 arm;
    shape_transform_point(node, world, pose, local, arm);
    const float dx = center.x - arm.x;
    const float dy = center.y - arm.y;
    const float dz = center.z - arm.z;
    radius = float(std::sqrt((static_cast<long double>(dz) * dz
        + static_cast<long double>(dy) * dy) + static_cast<long double>(dx) * dx));
    local = node.point;
    local.y = node.along_y + local.y;
    shape_transform_point(node, world, pose, local, arm);
    const float hx = center.x - arm.x;
    const float hy = center.y - arm.y;
    const float hz = center.z - arm.z;
    up = float(std::sqrt((static_cast<long double>(hz) * hz
        + static_cast<long double>(hy) * hy) + static_cast<long double>(hx) * hx));
    down = -up;
}

// Xbox 0x00055AA0
void transform_runtime_shape_cylinder(const EntityShapeNode& node, const float* world,
    const EntityBonePose* pose, u8* output)
{
    Vec3 center, direction;
    float radius, up, down;
    measure_runtime_shape_cylinder(node, world, pose, center, direction, radius, up, down);
    bound_header(output);
    bound_store(output, 4, center.x);
    bound_store(output, 8, center.y + down);
    bound_store(output, 0xC, center.z);
    bound_store(output, 0x10, 0.f);
    bound_store(output, 0x18, 0.f);
    bound_store(output, 0x14, up - down);
    bound_store(output, 0x1C, radius);
}

// Xbox 0x000426F0
i32 build_runtime_actor_bound(Actor& actor, u8* output, u32 mask, bool average,
    GhoulySpawnPool*& shapes)
{
    Vec3 center = actor_world_position(actor);
    const u8* block = actor_shape_block(actor, 4);
    if (block == nullptr)
    {
        const float radius = float(actor_model_bound_radius(actor));
        const float height = float(actor_model_height(actor));
        bound_header(output);
        bound_store(output, 4, center.x);
        bound_store(output, 8, center.y);
        bound_store(output, 0xC, center.z);
        bound_store(output, 0x14, height);
        bound_store(output, 0x10, 0.f);
        bound_store(output, 0x18, 0.f);
        bound_store(output, 0x1C, radius);
        return 0;
    }
    const EntityBonePose* pose = nullptr;
    if (actor.model != nullptr)
    {
        pose = actor.model->alternate_shape_pose;
        if (pose == nullptr)
        {
            pose = actor.model->primary_shape_pose;
        }
    }
    if (shapes == nullptr)
    {
        shapes = allocate_runtime_shape_pool(0x28, u16(count_runtime_actor_bound_shapes(actor, mask)));
    }
    else
    {
        shapes->cursor = shapes->begin;
    }
    Vec3 sum{0.f, 0.f, 0.f};
    const u8* record = block + 8;
    while (record != nullptr)
    {
        if ((read_u32({record, 8}, 4) & mask) != 0)
        {
            if (record[0] == 0 || record[0] == 1)
            {
                if (shapes->cursor == shapes->limit)
                {
                    shapes = grow_runtime_shape_pool(shapes);
                }
                u8* selected = shapes->cursor;
                shapes->cursor += shapes->stride;
                const auto& node = *reinterpret_cast<const EntityShapeNode*>(record);
                if (record[0] == 0)
                {
                    Vec3 point;
                    const float radius = shape_sphere_metrics(node, actor.xform, pose, point);
                    selected[0] = 1;
                    selected[1] = 0x14;
                    selected[2] = 1;
                    bound_store(selected, 4, point.x);
                    bound_store(selected, 8, point.y);
                    bound_store(selected, 0xC, point.z);
                    bound_store(selected, 0x10, radius);
                    sum.y = point.y + sum.y;
                }
                else
                {
                    transform_runtime_shape_cylinder(node, actor.xform, pose, selected);
                    sum.y = bound_float(selected, 0x14) + (bound_float(selected, 8) + sum.y);
                }
                sum.x = bound_float(selected, 4) + sum.x;
                sum.z = bound_float(selected, 0xC) + sum.z;
            }
        }
        if (record[1] == 0)
        {
            break;
        }
        record += record[1];
    }
    const i32 count = i32(shapes->cursor - shapes->begin) / shapes->stride;
    if (count == 0)
    {
        return 0;
    }
    if (!average)
    {
        center = actor_world_position(actor);
    }
    else
    {
        float divisor = float(count);
        if (count < 0)
        {
            divisor += 4294967296.f;
        }
        const float inverse = 1.f / divisor;
        center = Vec3{sum.x * inverse, sum.y * inverse, sum.z * inverse};
    }
    float radius = 0.f;
    float bottom = center.y;
    float top = center.y;
    for (const u8* shape = shapes->begin; shape < shapes->cursor; shape += 0x28)
    {
        if (shape[0] == 1 || shape[0] == 4)
        {
            const float dx = bound_float(shape, 4) - center.x;
            const float dz = bound_float(shape, 0xC) - center.z;
            const float extent = shape[0] == 1 ? bound_float(shape, 0x10) : bound_float(shape, 0x1C);
            const float reach = float(retail_square_root(dz * dz + dx * dx) + extent);
            if (radius < reach)
            {
                radius = reach;
            }
            const float low = shape[0] == 1 ? bound_float(shape, 8) - extent : bound_float(shape, 8);
            if (low <= bottom)
            {
                bottom = low;
            }
            const float high = (shape[0] == 1 ? extent : bound_float(shape, 0x14)) + bound_float(shape, 8);
            if (top < high)
            {
                top = high;
            }
        }
    }
    bound_header(output);
    bound_store(output, 4, center.x);
    bound_store(output, 8, bottom);
    bound_store(output, 0xC, center.z);
    bound_store(output, 0x10, 0.f);
    bound_store(output, 0x18, 0.f);
    bound_store(output, 0x14, top - bottom);
    bound_store(output, 0x1C, radius);
    return i32(shapes->cursor - shapes->begin) / shapes->stride;
}

// Xbox 0x0012B160
void update_runtime_actor_bound_record(std::span<u8> record, const u8* first,
    const u8* second, const Vec3& position, u8 value)
{
    copy_actor_shape(record.data() + 0x1C, first);
    copy_actor_shape(record.data() + 0x3C, second != nullptr ? second : first);
    std::memcpy(record.data() + 0x5C, &position.x, 4);
    std::memcpy(record.data() + 0x60, &position.y, 4);
    std::memcpy(record.data() + 0x64, &position.z, 4);
    record[0x19] = value;
}

// Xbox 0x00025C50
void apply_runtime_body_actor_bounds(std::span<const u8> body, Actor& actor,
    std::span<u8> actor_bytes, const EntityBitBlock& flags, std::span<u8>& bound,
    GhoulySpawnPool*& shapes)
{
    if (actor.model != nullptr && actor.model->cpu.data() != nullptr)
    {
        const Vec3 position = actor_world_position(actor);
        std::array<u8, 0x20> first;
        if (bound.data() != nullptr)
        {
            std::array<u8, 0x20> second;
            bound_header(second.data());
            bound_store(second.data(), 4, position.x);
            bound_store(second.data(), 8, position.y);
            bound_store(second.data(), 0xC, position.z);
            bound_store(second.data(), 0x10, 0.f);
            bound_store(second.data(), 0x14, actor.radius_y * get(body, body_field::height_scale));
            bound_store(second.data(), 0x18, 0.f);
            bound_store(second.data(), 0x1C, actor.radius_xz * get(body, body_field::radius_scale));
            if ((flags.bytes[7] & 1) != 0)
            {
                bound_header(first.data());
                bound_store(first.data(), 4, read_f32(bound, 0x20) + read_f32(body, 0x704));
                bound_store(first.data(), 8, read_f32(bound, 0x24) + read_f32(body, 0x708));
                bound_store(first.data(), 0xC, read_f32(bound, 0x28) + read_f32(body, 0x70C));
                bound_store(first.data(), 0x10, 0.f);
                bound_store(first.data(), 0x14, read_f32(bound, 0x30));
                bound_store(first.data(), 0x18, 0.f);
                bound_store(first.data(), 0x1C, read_f32(bound, 0x38));
            }
            else
            {
                build_runtime_actor_bound(actor, first.data(), read_u32(body, 0x890), true, shapes);
            }
            update_runtime_actor_bound_record(bound, first.data(), second.data(), actor.anchor, u8(read_u32(actor_bytes, 0xCEC)));
        }
        build_runtime_actor_bound(actor, first.data(), read_u32(body, 0x88C), false, shapes);
        const float inverse = 1.f / actor.radius_y;
        const float low = (bound_float(first.data(), 8) - position.y) * inverse;
        const float high = ((bound_float(first.data(), 0x14) + bound_float(first.data(), 8)) - position.y) * inverse;
        const float radius = bound_float(first.data(), 0x1C) / actor.radius_xz;
        // Actor +0x1C4..+0x1D0 are the native actor's model size fields, which every bound reader takes.
        actor.model_size_y = high; // +0x1C8
        actor.model_size_z = low; // +0x1CC
        actor.model_size_x = radius; // +0x1C4
        const float extent = !(high >= low) ? low : high;
        actor.model_extent = float(retail_square_root(extent * extent + radius * radius)); // +0x1D0
    }
}
}
