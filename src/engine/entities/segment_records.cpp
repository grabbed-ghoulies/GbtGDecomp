#include "segment_records.h"
#include "contact_sweep.h"
#include "body_shape_push.h"
#include "entity.h"
#include "entity_heap.h"
#include "gbtg/anim.h"
#include <bit>
#include <cstring>

namespace gbtg
{
void engine_copy_bytes(void* destination, const void* source, u32 count); // Xbox 0x000F71F0

// Xbox 0x000FC3F0
void separate_runtime_segment_contacts(const entity_detail::SegmentPool& pool,
    const Vec3& position, float radius, Vec3& output, const Vec3& stationary)
{
    output = position;
    i32 count = 0;
    const u8* end = pool.cursor;
    for (const u8* row = pool.begin; row < end; row += 0x54)
    {
        if (row[0x28] == 0)
        {
            count += separate_runtime_collision_triangle({row + 0x28, 0x2C}, output, radius);
        }
        else if (row[0x28] == 1)
        {
            count += separate_runtime_collision_sphere({row + 0x28, 0x14}, output, radius);
        }
    }
    if (count > 1)
    {
        Vec3 movement{position.x - output.x, position.y - output.y, position.z - output.z};
        for (const u8* row = pool.begin; row < end; row += 0x54)
        {
            float fraction;
            bool hit = false;
            if (row[0x28] == 0)
            {
                Vec3 first;
                Vec3 second;
                Vec3 third;
                Vec3 normal;
                std::memcpy(&first, row + 0x2C, 12);
                std::memcpy(&second, row + 0x38, 12);
                std::memcpy(&third, row + 0x44, 12);
                hit = sweep_runtime_collision_triangle(first, second, third, output,
                    movement, radius, normal, fraction, stationary);
            }
            else if (row[0x28] == 1)
            {
                Vec3 center;
                float sphere_radius;
                std::memcpy(&center, row + 0x2C, 12);
                std::memcpy(&sphere_radius, row + 0x38, 4);
                hit = sweep_runtime_collision_sphere_pair(output, radius, movement,
                    center, sphere_radius, stationary, 1.f, fraction);
                if (hit && !(fraction >= 0.f))
                {
                    hit = false;
                }
            }
            if (hit)
            {
                movement.x = movement.x * fraction;
                movement.y = movement.y * fraction;
                movement.z = movement.z * fraction;
                if (fraction == 0.f)
                {
                    break;
                }
            }
        }
        output.y = movement.y + output.y;
        output.x = output.x + movement.x;
        output.z = output.z + movement.z;
        Vec3 away{output.x - position.x, output.y - position.y, output.z - position.z};
        const float squared = (away.y * away.y + away.x * away.x) + away.z * away.z;
        if (squared != 0.f)
        {
            const long double reciprocal = retail_reciprocal_sqrt(squared);
            away.x = float(static_cast<long double>(away.x) * reciprocal);
            away.y = float(static_cast<long double>(away.y) * reciprocal);
            away.z = float(static_cast<long double>(away.z) * reciprocal);
        }
        constexpr float gap = std::bit_cast<float>(0x3A83126Fu);
        output.x = away.x * gap + output.x;
        output.y = away.y * gap + output.y;
        output.z = away.z * gap + output.z;
    }
}

// Xbox 0x000FCA60
void normalized_segment_contact_direction(const entity_detail::SegmentPool& pool, Vec3& output)
{
    const u32 used = u32(pool.cursor - pool.begin);
    u32 offset = 0;
    output = {0.f, 0.f, 0.f};
    if (i32(used + 0x53) / 0x54 >= 4)
    {
        u32 groups = (used - 0xFD) / 0x150 + 1;
        do
        {
            const std::span<const u8> rows{pool.begin + offset, 0x150};
            output.x = read_f32(rows, 0xC) + output.x;
            output.y = read_f32(rows, 0x10) + output.y;
            output.z = output.z + read_f32(rows, 0x14);
            output.x = read_f32(rows, 0x60) + output.x;
            output.y = read_f32(rows, 0x64) + output.y;
            output.z = read_f32(rows, 0x68) + output.z;
            output.x = read_f32(rows, 0xB4) + output.x;
            output.y = read_f32(rows, 0xB8) + output.y;
            output.z = read_f32(rows, 0xBC) + output.z;
            output.x = read_f32(rows, 0x108) + output.x;
            output.y = read_f32(rows, 0x10C) + output.y;
            output.z = read_f32(rows, 0x110) + output.z;
            offset += 0x150;
            --groups;
        }
        while (groups != 0);
    }
    if (offset < used)
    {
        u32 remaining = (used - offset - 1) / 0x54 + 1;
        do
        {
            const std::span<const u8> row{pool.begin + offset, 0x54};
            output.x = read_f32(row, 0xC) + output.x;
            output.y = read_f32(row, 0x10) + output.y;
            output.z = read_f32(row, 0x14) + output.z;
            offset += 0x54;
            --remaining;
        }
        while (remaining != 0);
    }
    const i32 count = i32(used) / i32(pool.stride);
    if (count != 0)
    {
        long double denominator = count;
        if (count < 0)
        {
            denominator += 4294967296.0L;
        }
        const long double reciprocal = 1.0L / denominator;
        output.x = float(reciprocal * static_cast<long double>(output.x));
        output.y = float(reciprocal * static_cast<long double>(output.y));
        output.z = float(reciprocal * static_cast<long double>(output.z));
    }
    const float squared = (output.z * output.z + output.y * output.y) + output.x * output.x;
    if (squared != 0.f)
    {
        const long double reciprocal = retail_reciprocal_sqrt(squared);
        output.x = float(reciprocal * static_cast<long double>(output.x));
        output.y = float(reciprocal * static_cast<long double>(output.y));
        output.z = float(reciprocal * static_cast<long double>(output.z));
    }
}

// Xbox 0x000FCC90
GhoulySpawnPool* copy_segment_records(const entity_detail::SegmentPool& pool)
{
    const i32 used = i32(pool.cursor - pool.begin) / i32(pool.stride);
    const u32 count = u32(used) & 0xFFFF;
    auto* copy = static_cast<GhoulySpawnPool*>(engine_heap_allocate(
        i32(sizeof(GhoulySpawnPool) + count * 0x54)));
    copy->stride = 0x54;
    u32 grow = count;
    if (u16(used) == 0)
    {
        grow = 10;
    }
    u8* rows = reinterpret_cast<u8*>(copy + 1);
    copy->begin = rows;
    copy->cursor = rows;
    copy->grow = u16(grow);
    copy->limit = rows + count * 0x54;
    const u8* end = pool.cursor;
    for (const u8* source = pool.begin; source < end; source += 0x54)
    {
        if (copy->cursor == copy->limit)
        {
            const i32 kept = i32(copy->cursor - copy->begin) / i32(copy->stride);
            const i32 capacity = i32(copy->grow) + kept;
            copy = static_cast<GhoulySpawnPool*>(engine_heap_reallocate(copy,
                i32(sizeof(GhoulySpawnPool)) + i32(copy->stride) * capacity));
            u8* grown = reinterpret_cast<u8*>(copy + 1);
            copy->cursor = grown + size_t(copy->stride) * kept;
            copy->limit = grown + size_t(copy->stride) * capacity;
            copy->begin = grown;
        }
        u8* target = copy->cursor;
        copy->cursor = target + copy->stride;
        entity_copy_match_block(target, source);
    }
    return copy;
}

// Xbox 0x000FC6C0
void nearest_segment_contact(const GhoulySpawnPool& records, Vec3& output, const Vec3& reference)
{
    float best = std::bit_cast<float>(0x7F7FFFFFu); // DAT_0032B310.
    const u8* row = records.begin;
    const u8* end = records.cursor;
    output = reference;
    if (row < end)
    {
        u32 remaining = u32(end - row - 1) / 0x54 + 1;
        do
        {
            if (row[0x28] == 1)
            {
                Vec3 centre;
                float radius;
                std::memcpy(&centre, row + 0x2C, sizeof(centre));
                std::memcpy(&radius, row + 0x38, 4);
                float dx = reference.x - centre.x;
                float dz = reference.z - centre.z;
                float dy = reference.y - centre.y;
                const float length = dz * dz + dx * dx + dy * dy;
                if (length != 0.0f)
                {
                    const long double scale = retail_reciprocal_sqrt(length) * radius;
                    dx = float(dx * scale);
                    dy = float(dy * scale);
                    dz = float(dz * scale);
                }
                const float z = dz + centre.z;
                const float x = centre.x + dx;
                const float y = centre.y + dy;
                const float ez = z - reference.z;
                const float ey = y - reference.y;
                const float ex = x - reference.x;
                const float distance = ez * ez + ey * ey + ex * ex;
                if (best > distance)
                {
                    output.x = x;
                    output.y = y;
                    output.z = z;
                    best = distance;
                }
            }
            row += 0x54;
            --remaining;
        } while (remaining != 0);
    }
}

// Xbox 0x000FC840, EDI pool
void cull_back_facing_segment_contacts(entity_detail::SegmentPool& pool, const Vec3& point)
{
    u8* end = pool.cursor;
    for (u8* row = pool.begin; row < end; row += 0x54)
    {
        if (row[0x28] != 0)
        {
            continue;
        }
        const auto f = [&](size_t offset)
        {
            float value;
            std::memcpy(&value, row + offset, 4);
            return value;
        };
        float dx = f(0x2C) - point.x;
        float dy = f(0x30) - point.y;
        float dz = f(0x34) - point.z;
        const float length = (dz * dz + dy * dy) + dx * dx;
        if (length != 0.f)
        {
            const long double scale = engine_reciprocal_sqrt(length); // 0x001534C0
            dx = float(dx * scale);
            dy = float(dy * scale);
            dz = float(dz * scale);
        }
        const float b_y = f(0x48) - f(0x30);
        const float b_z = f(0x4C) - f(0x34);
        const float a_y = f(0x3C) - f(0x30);
        const float a_z = f(0x40) - f(0x34);
        const float a_x = f(0x38) - f(0x2C);
        const float b_x = f(0x44) - f(0x2C);
        float nx = a_z * b_x - b_z * a_x;
        float ny = b_y * a_x - a_y * b_x;
        float nz = b_z * a_y - b_y * a_z;
        const float normal = (ny * ny + nx * nx) + nz * nz;
        if (normal != 0.f)
        {
            const long double scale = engine_reciprocal_sqrt(normal);
            nz = float(nz * scale);
            nx = float(nx * scale);
            ny = float(ny * scale);
        }
        if (ny * dz + nx * dy + nz * dx >= 0.f)
        {
            const u8* last = pool.cursor - pool.stride;
            if (row < last)
            {
                engine_copy_bytes(row, last, pool.stride); // 0x000F71F0
            }
            pool.cursor -= pool.stride;
            row -= 0x54;
            end -= 0x54;
        }
    }
}
}
