#include "body_collision.h"
#include "actor_transform.h"
#include "entity.h"
#include "entity_collision.h"
#include "shape_transform.h"
#include "gbtg/anim.h"
#include "segment_records.h"
#include "entity_heap.h"
#include "../navigation/navigation.h"
#include "body_fields.h"
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
void put_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

float record_float(const u8* record, size_t offset)
{
    float value;
    std::memcpy(&value, record + offset, 4);
    return value;
}
}

// Xbox 0x000755A0
bool separate_runtime_player_world_contacts(std::span<const u8> body, Actor& actor,
    const EntityMaskHost& mask, const Vec3& stationary)
{
    Vec3 position = actor_world_position(actor);
    const float radius = read_f32(body, 0x894) * actor.radius_xz;
    const float height = (read_f32(body, 0x8A4) * actor.radius_xz + radius) + 0.5f;
    Vec3 probe{position.x, height + position.y, position.z};
    const u32 collision_mask = camera_collision_mask(mask);
    if (entity_collect_sphere_contacts(probe, radius, 4, collision_mask) != 0)
    {
        auto contacts = entity_merge_contact_records();
        Vec3 separated;
        separate_runtime_segment_contacts(contacts, probe, radius, separated, stationary);
        position.x = (separated.x - probe.x) + position.x;
        position.y = (separated.y - probe.y) + position.y;
        position.z = (separated.z - probe.z) + position.z;
        actor_set_world_position(actor, position);
        engine_heap_free(contacts.block);
        return true;
    }
    return false;
}

// Xbox 0x000744F0
void separate_runtime_player_body_contacts(std::span<u8>& body, Actor& actor,
    EntityBitBlock& bits, const EntityMaskHost& mask, i32 floor, const Vec3& stationary)
{
    Vec3 position = get(body, body_field::position);
    const float radius = read_f32(body, 0x894) * actor.radius_xz;
    const float height = (read_f32(body, 0x8A4) * actor.radius_xz + radius) + 0.5f;
    Vec3 probe{position.x, position.y + height, position.z};
    const u32 collision_mask = camera_collision_mask(mask);
    if (entity_collect_sphere_contacts(probe, radius, 4, collision_mask) > 0)
    {
        auto contacts = entity_merge_contact_records();
        Vec3 separated;
        separate_runtime_segment_contacts(contacts, probe, radius, separated, stationary);
        engine_heap_free(contacts.block);
        position.x = (separated.x - probe.x) + position.x;
        position.y = (separated.y - probe.y) + position.y;
        position.z = (separated.z - probe.z) + position.z;
        actor_set_world_position(actor, position);
        const Vec3 world = actor_world_position(actor);
        std::memcpy(body.data() + 0x18, &world, 12);
        sweep_runtime_player_body_floor(body, actor, bits, mask, floor,
            position, position, probe, radius, height);
        bits.bytes[2] |= 0x20;
        return;
    }
    bits.bytes[2] &= 0xDF;
}

// Xbox 0x00075350
bool sweep_runtime_player_body_floor(std::span<u8> body, Actor& actor,
    EntityBitBlock& bits, const EntityMaskHost& mask, i32 floor,
    const Vec3& start, const Vec3& base, Vec3& candidate, float radius, float height)
{
    float minimum = base.y;
    if (start.y >= minimum)
    {
        minimum = start.y;
    }
    minimum = minimum + actor.radius_y * read_f32(body, 0x8A4);
    Vec3 upper{base.x, candidate.y - radius, base.z};
    if (!(upper.y >= minimum))
    {
        upper.y = minimum;
    }
    Vec3 lower{upper.x, upper.y - (height + 50.f), upper.z}; // DAT_0032B1E0.
    bits.bytes[5] &= 0xFD;
    if (entity_record_sweep_probe(lower, upper, 8, camera_collision_mask(mask)) == 0)
    {
        body[0x19E] = 0;
        return false;
    }
    entity_copy_match_block(body.data() + 0x178, entity_detail::g_segment_slots[0].pool.begin);
    const Vec3 surface{read_f32(body, 0x178), read_f32(body, 0x17C), read_f32(body, 0x180)};
    const Vec3 anchor = actor.anchor;
    const float difference = base.y - surface.y;
    if (surface.y > minimum)
    {
        bits.bytes[5] |= 2;
        u8 kind = 0;
        const auto* grid = entity_detail::g_nav_grids[floor];
        if (grid != nullptr)
        {
            const i32 cell = entity_detail::navigation_cell_index(floor, surface);
            if (cell != -1)
            {
                const u8* record = reinterpret_cast<const u8*>(&grid->cells[cell]);
                kind = record[0xB];
                if (kind == 0)
                {
                    kind = record[0xA];
                }
            }
        }
        body[0x874] = kind;
        candidate.y = minimum + height;
        actor.anchor = Vec3{anchor.x, 0.f, anchor.z};
        return true;
    }
    constexpr float low = std::bit_cast<float>(0xBA83126Fu); // DAT_0032B490.
    constexpr float high = std::bit_cast<float>(0x3A83126Fu); // DAT_0032B1A4.
    if ((difference < 0.f || (difference >= low && difference <= high))
        && anchor.y <= 0.f)
    {
        bits.bytes[5] |= 2;
        body[0x874] = entity_detail::navigation_surface_kind(floor, surface);
        candidate.y = surface.y + height;
        actor.anchor = Vec3{anchor.x, 0.f, anchor.z};
    }
    return true;
}

// Xbox 0x00025A30
i32 record_runtime_body_motion(std::span<u8> history, const Vec3& delta, const Actor& owner,
    const RuntimeActorClock& clock)
{
    const float step = runtime_frame_step(clock);
    const float rate = runtime_actor_rate(owner);
    size_t sample = 4 + size_t(read_u32(history, 0)) * 12;
    put_float(history, sample, delta.x);
    put_float(history, sample + 4, delta.y);
    put_float(history, sample + 8, delta.z);
    const float scale = rate * step;
    if (scale > 0.0f)
    {
        const float inverse = 1.0f / scale; // DAT_0032B0F8.
        sample = 4 + size_t(read_u32(history, 0)) * 12;
        put_float(history, sample, read_f32(history, sample) * inverse);
        put_float(history, sample + 4, read_f32(history, sample + 4) * inverse);
        put_float(history, sample + 8, read_f32(history, sample + 8) * inverse);
    }
    const i32 next = i32(read_u32(history, 0)) + 1;
    const u32 index = u32(next % 0x50);
    std::memcpy(history.data(), &index, 4);
    return next / 0x50;
}

// Xbox 0x00026100
u8 push_runtime_body_sphere(RuntimeBodyCollision& context, RuntimeBodyCollisionSphere& sphere,
    const float* world)
{
    Actor& owner = context.owner;
    sphere.contact[0x26] = 0;
    const EntityBonePose* pose;
    if (owner.model == nullptr)
    {
        pose = nullptr;
    }
    else
    {
        pose = owner.model->alternate_shape_pose;
        if (pose == nullptr)
        {
            pose = owner.model->primary_shape_pose;
        }
    }
    Vec3 origin;
    const float radius = shape_sphere_metrics(*sphere.node, world, pose, origin);
    Vec3 center = origin;
    Vec3 target = actor_world_position(owner);
    target.y = float(static_cast<long double>(target.y) + radius + 1.0L); // DAT_0032B0F8.
    const u32 mask = camera_collision_mask(context.mask);
    Vec3 kept;
    for (i32 pass = 0; pass < 3; ++pass)
    {
        kept = center;
        const u8 kind = context.body[0x717];
        i32 hit;
        if (kind == 0)
        {
            hit = entity_record_movement_probe(kept, target, std::bit_cast<u32>(radius), 0xC, mask);
        }
        else if (kind == 1)
        {
            float dx = center.x - target.x;
            float dy = center.y - target.y;
            float dz = center.z - target.z;
            const float length = dz * dz + dy * dy + dx * dx;
            if (length != 0.0f)
            {
                const long double scale = retail_reciprocal_sqrt(length);
                dx = float(dx * scale);
                dy = float(dy * scale);
                dz = float(dz * scale);
            }
            kept.x = dx * radius + kept.x;
            kept.y = dy * radius + kept.y;
            kept.z = dz * radius + kept.z;
            hit = entity_record_sweep_probe(kept, target, 0xC, mask);
        }
        else
        {
            break;
        }
        if (hit == 0)
        {
            break;
        }
        // DAT_00511A20: the first slot's pool; its first record is at pool +4.
        const u8* record = entity_detail::g_segment_slots[0].pool.begin;
        if (sphere.contact[0x26] == 0)
        {
            entity_copy_match_block(sphere.contact.data(), record);
        }
        const float normal_x = record_float(record, 0xC);
        const float normal_y = record_float(record, 0x10);
        const float normal_z = record_float(record, 0x14);
        const float dot = ((center.z - target.z) - (kept.z - target.z)) * normal_z +
            ((center.y - target.y) - (kept.y - target.y)) * normal_y +
            ((center.x - target.x) - (kept.x - target.x)) * normal_x;
        float push = 0.0f - dot;
        constexpr float kMinimumPush = std::bit_cast<float>(0x3D4CCCCDu); // DAT_0032B250.
        bool apply;
        if (kMinimumPush > push)
        {
            push = kMinimumPush;
            apply = true;
        }
        else
        {
            apply = !(0.0f > push);
        }
        if (apply)
        {
            center.x = push * normal_x + center.x;
            center.y = normal_y * push + center.y;
            center.z = normal_z * push + center.z;
        }
    }
    if (sphere.contact[0x26] == 0)
    {
        return 0;
    }
    float move_x = kept.x - origin.x;
    float move_y = kept.y - origin.y;
    float move_z = kept.z - origin.z;
    if ((context.bits->bytes[5] & 2) != 0)
    {
        move_y = 0.0f;
    }
    const float step = runtime_frame_step(context.clock);
    const float rate = runtime_actor_rate(owner);
    constexpr float kLateralRate = 20.0f; // DAT_003390C8.
    const float limit = float(static_cast<long double>(step) * rate * kLateralRate);
    const float lateral_x = move_x * move_x;
    const float lateral_z = move_z * move_z;
    if (retail_square_root(lateral_x + lateral_z) > limit)
    {
        const float lateral = lateral_x + lateral_z;
        if (lateral != 0.0f)
        {
            const long double scale = retail_reciprocal_sqrt(lateral) * limit;
            move_x = float(move_x * scale);
            move_z = float(move_z * scale);
        }
    }
    std::span<u8> body = context.body;
    put_float(body, 0x18, get(body, body_field::position).x + move_x);
    put_float(body, 0x1C, get(body, body_field::position).y + move_y);
    put_float(body, 0x20, get(body, body_field::position).z + move_z);
    return 1;
}

// Xbox 0x00025F00
void push_runtime_body_spheres(RuntimeBodyCollision& context)
{
    std::span<u8> body = context.body;
    if (context.spheres != nullptr && body[0x715] != 0)
    {
        Vec3 position = get(body, body_field::position);
        float world[12];
        actor_build_world_matrix(context.owner, world, &position);
        RuntimeBodyCollisionSphere* end = context.spheres + body[0x714];
        body[0x716] = 0;
        for (RuntimeBodyCollisionSphere* sphere = context.spheres; sphere < end; ++sphere)
        {
            const u8 pushed = push_runtime_body_sphere(context, *sphere, world);
            body[0x716] = u8(body[0x716] + pushed);
        }
    }
}

// Xbox 0x00025FE0
RuntimeBodyCollisionSphere* first_hit_runtime_body_sphere(std::span<const u8> body,
    RuntimeBodyCollisionSphere* spheres)
{
    RuntimeBodyCollisionSphere* end = spheres + body[0x714];
    for (RuntimeBodyCollisionSphere* sphere = spheres; sphere < end; ++sphere)
    {
        if (sphere->contact[0x26] != 0)
        {
            return sphere;
        }
    }
    return nullptr;
}
}
