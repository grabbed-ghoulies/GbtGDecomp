#include "effect_collision_response.h"
#include "../entities/entity_collision.h"
#include "../navigation/navigation.h"
#include "../entities/direction_pitch.h"
#include "../entities/player_mind_events.h"
#include "../entities/euler_wrapping.h"
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
void store_response_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}
float response_fraction(u32& random)
{
    random = random * 0x19660Du + 0x3C6EF35Fu;
    return std::bit_cast<float>((random & 0x7FFFFFu) | 0x3F800000u) - 1.f;
}
}

// Xbox 0x000ECE50
bool move_actor_effect_particle(std::span<u8> particle, RuntimeEffectCollisionContext& context,
    const std::function<void()>& hit_callback)
{
    const float step = runtime_frame_step(context.clock);
    if (particle[0x54] != 0)
    {
        return false;
    }
    Vec3 source{read_f32(particle, 0), read_f32(particle, 4), read_f32(particle, 8)};
    Vec3 candidate{source.x + read_f32(particle, 0x18) * step,
        source.y + read_f32(particle, 0x1C) * step,
        source.z + read_f32(particle, 0x20) * step};
    bool hit = false;
    if ((particle[0x56] & 0x10) != 0)
    {
        hit = clip_runtime_camera_plane(context.view, context.plane, source, candidate,
            &context.collision.normal, nullptr);
        if (hit)
        {
            context.collision.kind = 2;
            context.collision.surface = 0;
            particle[0x55] = 0;
        }
    }
    if (!hit)
    {
        hit = probe_actor_effect_particle(particle, candidate, context.collision);
        if (!hit)
        {
            particle[0x55] = u8(particle[0x55] + 1);
            context.collision.kind = 0;
        }
    }
    if (hit)
    {
        std::memcpy(particle.data(), &candidate, 12);
        if (hit_callback)
        {
            hit_callback();
        }
        reflect_actor_effect_particle(particle, context.collision.flags, context.collision.normal, context.random);
    }
    bool outside = false;
    if (context.bounded != 0)
    {
        float top = context.maximum.y;
        if (context.maximum.y < candidate.y)
        {
            top = candidate.y;
        }
        if (candidate.x < context.minimum.x || candidate.x >= context.maximum.x
            || candidate.y < context.minimum.y || candidate.y >= top + 1.f
            || candidate.z < context.minimum.z || candidate.z >= context.maximum.z)
        {
            particle[0x54] = 1;
            outside = true;
        }
    }
    if (!outside)
    {
        std::memcpy(particle.data(), &candidate, 12);
    }
    Vec3 velocity{read_f32(particle, 0x18), read_f32(particle, 0x1C), read_f32(particle, 0x20)};
    if ((particle[0x56] & 2) != 0)
    {
        float pitch;
        if (runtime_direction_pitch(velocity, pitch))
        {
            store_response_float(particle, 0xC, pitch);
        }
    }
    if ((particle[0x56] & 8) != 0)
    {
        float yaw;
        if (runtime_direction_angle(velocity, yaw))
        {
            store_response_float(particle, 0x10, yaw);
        }
    }
    Vec3 euler{read_f32(particle, 0x24) * step + read_f32(particle, 0xC),
        read_f32(particle, 0x28) * step + read_f32(particle, 0x10),
        read_f32(particle, 0x2C) * step + read_f32(particle, 0x14)};
    std::memcpy(particle.data() + 0xC, &euler, 12);
    wrap_runtime_euler_angles(euler);
    std::memcpy(particle.data() + 0xC, &euler, 12);
    store_response_float(particle, 0x1C, read_f32(particle, 0x4C) * step + read_f32(particle, 0x1C));
    if (read_f32(particle, 0x50) != 0.f)
    {
        float drag = read_f32(particle, 0x50) * step;
        if (!(1.f > drag))
        {
            drag = 1.f;
        }
        const float factor = 1.f - drag;
        store_response_float(particle, 0x18, factor * read_f32(particle, 0x18));
        store_response_float(particle, 0x1C, read_f32(particle, 0x1C) * factor);
        store_response_float(particle, 0x20, read_f32(particle, 0x20) * factor);
    }
    return context.collision.kind != 0;
}

// Xbox 0x000ECA20
i32 probe_actor_effect_world(Vec3& source, Vec3& candidate, float radius, i32 kind,
    u32 mask, u32 grid_only, RuntimeEffectCollision& collision)
{
    if (grid_only == 0)
    {
        i32 hit;
        if (radius == 0.f)
        {
            hit = entity_record_sweep_probe(source, candidate, kind, mask);
        }
        else
        {
            hit = entity_record_movement_probe(source, candidate, std::bit_cast<u32>(radius), kind, mask);
        }
        if (hit != 0)
        {
            const auto record = std::span<const u8>{entity_detail::g_segment_slots[0].pool.begin, 0x54};
            collision.surface = record[0x1E];
            collision.flags = read_u32(record, 0x20);
            collision.normal = {read_f32(record, 0xC), read_f32(record, 0x10), read_f32(record, 0x14)};
            return hit;
        }
    }
    else
    {
        const i32 floor = entity_detail::navigation_floor_index(candidate, -1, 0);
        const i32 cell_index = entity_detail::navigation_cell_index(floor, candidate);
        const auto* grid = entity_detail::g_nav_grids[floor];
        if (cell_index != -1)
        {
            const auto& cell = grid->cells[cell_index];
            const float source_y = source.y;
            const float source_z = source.z;
            const float height = ((source.z + source.x) * 0.f + source.y) - float(cell.bottom);
            if (!(0.f > height))
            {
                const float dx = candidate.x - source.x;
                const float dy = candidate.y - source_y;
                const float dz = candidate.z - source_z;
                const float travel = (dz + dx) * 0.f - dy;
                if (!(height > travel))
                {
                    float fraction = height / travel;
                    if (travel == 0.f)
                    {
                        fraction = 0.f;
                    }
                    candidate.x = source.x + dx * fraction;
                    candidate.y = dy * fraction + source.y;
                    candidate.z = source.z + dz * fraction;
                    const auto* bytes = reinterpret_cast<const u8*>(&cell);
                    collision.surface = bytes[0xB];
                    if (collision.surface == 0)
                    {
                        collision.surface = bytes[0xA];
                    }
                    collision.flags = 8;
                    collision.normal = {0.f, 1.f, 0.f};
                    return 1;
                }
            }
        }
    }
    return 0;
}

// Xbox 0x000ECBD0
bool probe_actor_effect_particle(std::span<u8> particle, Vec3& candidate,
    RuntimeEffectCollision& collision)
{
    const u8 flags = particle[0x56];
    if ((flags & 1) != 0)
    {
        Vec3 source{read_f32(particle, 0), read_f32(particle, 4), read_f32(particle, 8)};
        const float radius = read_f32(particle, 0x48) * read_f32(particle, 0x3C);
        const i32 hit = probe_actor_effect_world(source, candidate, radius, 0, 0xC, flags & 0x20, collision);
        std::memcpy(particle.data(), &source, 12);
        if (hit != 0)
        {
            if (particle[0x55] == 0)
            {
                particle[0x54] = 1;
            }
            collision.kind = 1;
            particle[0x55] = 0;
            return true;
        }
    }
    return false;
}

// Xbox 0x000ECC30
void reflect_actor_effect_particle(std::span<u8> particle, u32 surface_flags,
    const Vec3& normal, u32& random)
{
    if ((surface_flags & 8) != 0)
    {
        const float factor = read_f32(particle, 0x40);
        for (size_t offset : {0x18u, 0x1Cu, 0x20u})
        {
            store_response_float(particle, offset, read_f32(particle, offset) * factor);
        }
    }
    if ((particle[0x56] & 4) != 0)
    {
        const float extent = ((read_f32(particle, 0x2C) + read_f32(particle, 0x28))
            + read_f32(particle, 0x24)) * .5f;
        const float lower = 0.f - extent;
        for (size_t offset : {0x24u, 0x28u, 0x2Cu})
        {
            const float fraction = response_fraction(random);
            store_response_float(particle, offset, (extent - lower) * fraction + lower);
        }
    }
    else
    {
        const float factor = read_f32(particle, 0x44);
        for (size_t offset : {0x24u, 0x28u, 0x2Cu})
        {
            store_response_float(particle, offset, read_f32(particle, offset) * factor);
        }
    }
    const float x = read_f32(particle, 0x18) * -1.f;
    const float y = read_f32(particle, 0x1C) * -1.f;
    const float z = read_f32(particle, 0x20) * -1.f;
    const float projection = ((normal.x * x + normal.y * y) + normal.z * z) * 2.f;
    store_response_float(particle, 0x18, normal.x * projection);
    store_response_float(particle, 0x1C, normal.y * projection);
    const float reflected_z = normal.z * projection;
    store_response_float(particle, 0x18, read_f32(particle, 0x18) - x);
    store_response_float(particle, 0x1C, read_f32(particle, 0x1C) - y);
    store_response_float(particle, 0x20, reflected_z);
    store_response_float(particle, 0x20, reflected_z - z);
}
}
