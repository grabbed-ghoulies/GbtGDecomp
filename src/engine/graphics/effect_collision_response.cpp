#include "effect_collision_response.h"
#include "../entities/entity_collision.h"
#include "../navigation/navigation.h"
#include "../entities/direction_pitch.h"
#include "../entities/player_mind_events.h"
#include "../entities/euler_wrapping.h"
#include "effect_particles.h"
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
float response_fraction(u32& random)
{
    random = random * 0x19660Du + 0x3C6EF35Fu;
    return std::bit_cast<float>((random & 0x7FFFFFu) | 0x3F800000u) - 1.f;
}
}

// Xbox 0x000ECE50
bool move_actor_effect_particle(EffectParticle& particle, RuntimeEffectCollisionContext& context,
    const std::function<void()>& hit_callback)
{
    const float step = runtime_frame_step(context.clock);
    if (particle.stopped != 0)
    {
        return false;
    }
    Vec3 source = particle.position;
    Vec3 candidate{source.x + particle.velocity.x * step,
        source.y + particle.velocity.y * step,
        source.z + particle.velocity.z * step};
    bool hit = false;
    if ((particle.motion_flags & 0x10) != 0)
    {
        hit = clip_runtime_camera_plane(context.view, context.plane, source, candidate,
            &context.collision.normal, nullptr);
        if (hit)
        {
            context.collision.kind = 2;
            context.collision.surface = 0;
            particle.unk_55 = 0;
        }
    }
    if (!hit)
    {
        hit = probe_actor_effect_particle(particle, candidate, context.collision);
        if (!hit)
        {
            particle.unk_55 = u8(particle.unk_55 + 1);
            context.collision.kind = 0;
        }
    }
    if (hit)
    {
        particle.position = candidate;
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
            particle.stopped = 1;
            outside = true;
        }
    }
    if (!outside)
    {
        particle.position = candidate;
    }
    Vec3 velocity = particle.velocity;
    if ((particle.motion_flags & 2) != 0)
    {
        float pitch;
        if (runtime_direction_pitch(velocity, pitch))
        {
            particle.euler.x = pitch;
        }
    }
    if ((particle.motion_flags & 8) != 0)
    {
        float yaw;
        if (runtime_direction_angle(velocity, yaw))
        {
            particle.euler.y = yaw;
        }
    }
    Vec3 euler{particle.spin.x * step + particle.euler.x,
        particle.spin.y * step + particle.euler.y,
        particle.spin.z * step + particle.euler.z};
    particle.euler = euler;
    wrap_runtime_euler_angles(euler);
    particle.euler = euler;
    particle.velocity.y = particle.gravity * step + particle.velocity.y;
    if (particle.drag != 0.f)
    {
        float drag = particle.drag * step;
        if (!(1.f > drag))
        {
            drag = 1.f;
        }
        const float factor = 1.f - drag;
        particle.velocity.x = factor * particle.velocity.x;
        particle.velocity.y = particle.velocity.y * factor;
        particle.velocity.z = particle.velocity.z * factor;
    }
    return context.collision.kind != 0;
}

// Xbox 0x000ECA20
i32 probe_actor_effect_world(Vec3& source, Vec3& candidate, float radius, i32 kind,
    u32 mask, u32 grid_only, RuntimeEffectCollision& collision)
{
    if (grid_only == 0)
    {
        // EBX (the probes' kept point) is the candidate and the first stack word is the source. ECX (`kind`) is
        // pushed before EDX (`mask`), so the probes take EDX as their kind and ECX as their mask.
        i32 hit;
        if (radius == 0.f)
        {
            hit = entity_record_sweep_probe(candidate, source, i32(mask), u32(kind));
        }
        else
        {
            hit = entity_record_movement_probe(candidate, source, std::bit_cast<u32>(radius), i32(mask), u32(kind));
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
bool probe_actor_effect_particle(EffectParticle& particle, Vec3& candidate,
    RuntimeEffectCollision& collision)
{
    const u8 flags = particle.motion_flags;
    if ((flags & 1) != 0)
    {
        Vec3 source = particle.position;
        const float radius = particle.size * particle.collision_radius;
        const i32 hit = probe_actor_effect_world(source, candidate, radius, 0, 0xC, flags & 0x20, collision);
        particle.position = source;
        if (hit != 0)
        {
            if (particle.unk_55 == 0)
            {
                particle.stopped = 1;
            }
            collision.kind = 1;
            particle.unk_55 = 0;
            return true;
        }
    }
    return false;
}

// Xbox 0x000ECC30
void reflect_actor_effect_particle(EffectParticle& particle, u32 surface_flags,
    const Vec3& normal, u32& random)
{
    if ((surface_flags & 8) != 0)
    {
        const float factor = particle.bounce;
        particle.velocity.x = particle.velocity.x * factor;
        particle.velocity.y = particle.velocity.y * factor;
        particle.velocity.z = particle.velocity.z * factor;
    }
    if ((particle.motion_flags & 4) != 0)
    {
        const float extent = ((particle.spin.z + particle.spin.y) + particle.spin.x) * .5f;
        const float lower = 0.f - extent;
        for (float* axis : {&particle.spin.x, &particle.spin.y, &particle.spin.z})
        {
            const float fraction = response_fraction(random);
            *axis = (extent - lower) * fraction + lower;
        }
    }
    else
    {
        const float factor = particle.spin_bounce;
        particle.spin.x = particle.spin.x * factor;
        particle.spin.y = particle.spin.y * factor;
        particle.spin.z = particle.spin.z * factor;
    }
    const float x = particle.velocity.x * -1.f;
    const float y = particle.velocity.y * -1.f;
    const float z = particle.velocity.z * -1.f;
    const float projection = ((normal.x * x + normal.y * y) + normal.z * z) * 2.f;
    particle.velocity.x = normal.x * projection;
    particle.velocity.y = normal.y * projection;
    const float reflected_z = normal.z * projection;
    particle.velocity.x = particle.velocity.x - x;
    particle.velocity.y = particle.velocity.y - y;
    particle.velocity.z = reflected_z;
    particle.velocity.z = reflected_z - z;
}
}
