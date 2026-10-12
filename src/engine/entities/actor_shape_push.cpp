#include "actor_shape_push.h"
#include "actor_transform.h"
#include "actor_runtime_queries.h"
#include "contact_sweep.h"
#include "entity.h"
#include <bit>
#include <cmath>

namespace gbtg
{
// Xbox 0x0003DD50
void runtime_actor_cylinder_metrics(const Actor& actor, Vec3& position,
    float& height, float& radius)
{
    position = actor_world_position(actor);
    position.y = actor.radius_y * actor.model_size_z + position.y;
    const float vertical = float((static_cast<long double>(actor.model_size_y)
        - actor.model_size_z) * actor.radius_y);
    height = engine_absolute(vertical);
    radius = engine_absolute(float(static_cast<long double>(actor.radius_xz) * actor.model_size_x));
}

// Xbox 0x001206A0
bool runtime_offset_cylinders_apart(const EntityCylinderBound& fixed,
    const EntityCylinderBound& moving, const Vec3& offset)
{
    const float y = moving.y + offset.y;
    if (y > fixed.height + fixed.y || fixed.y > moving.height + y)
    {
        return true;
    }
    const float dz = (moving.z + offset.z) - fixed.z;
    const float radius = fixed.radius + moving.radius;
    const float dx = (moving.x + offset.x) - fixed.x;
    const long double metric = retail_square_root(dz * dz + dx * dx);
    float distance = float(metric);
    if (static_cast<long double>(std::bit_cast<float>(0x3727C5ACu)) > metric)
    {
        distance = radius - std::bit_cast<float>(0x3C23D70Au);
    }
    // COMISS distance,radius; JC also accepts an unordered comparison.
    return !(distance < radius || std::isnan(distance) || std::isnan(radius));
}

// Xbox 0x000418B0
void runtime_actor_planar_delta(const Actor& first, const Actor& second, Vec3& output)
{
    const Vec3 a = actor_world_position(first);
    const Vec3 b = actor_world_position(second);
    output.x = b.x - a.x;
    output.z = b.z - a.z;
    output.y = 0.f;
}

// Xbox 0x00120BA0
void sweep_runtime_directed_cylinder_push(const Vec3& moving, const Vec3& fixed,
    float moving_radius, float fixed_radius, const Vec3& direction, Vec3& output,
    const std::function<void(Vec3&, const Vec3&)>& normalize)
{
    Vec3 begin{moving.x, 0.f, moving.z};
    const Vec3 obstacle{fixed.x, 0.f, fixed.z};
    Vec3 unit;
    normalize(unit, direction);
    begin = {unit.x * 50.f + begin.x, unit.y * 50.f + begin.y, unit.z * 50.f + begin.z};
    const Vec3 movement{unit.x * -50.f, unit.y * -50.f, unit.z * -50.f};
    const Vec3 stationary{};
    float fraction;
    if (sweep_runtime_collision_sphere_pair(begin, moving_radius, movement, obstacle,
        fixed_radius, stationary, 1.f, fraction) && fraction > 0.f)
    {
        output.x = (movement.x * fraction + begin.x) - moving.x;
        output.y = (movement.y * fraction + begin.y) - moving.y;
        output.z = (movement.z * fraction + begin.z) - moving.z;
        output.y = 0.f;
        return;
    }
    output.z = 0.f;
    output.x = 0.f;
    output.y = 0.f;
}

// Xbox 0x00120950
bool push_runtime_attached_cylinder(const Actor& first, const Actor& second,
    const EntityCylinderBound& fixed, const EntityCylinderBound& moving,
    const Vec3* query, const Vec3& offset, Vec3& push, Vec3& contact,
    const std::function<void(Vec3&, const Vec3&)>& normalize)
{
    const Vec3 point{moving.x + offset.x, moving.y + offset.y, moving.z + offset.z};
    if (point.y > fixed.y + fixed.height || fixed.y > moving.height + point.y)
    {
        return false;
    }
    const float dz = point.z - fixed.z;
    const float radius = fixed.radius + moving.radius;
    const float dx = point.x - fixed.x;
    const long double metric = retail_square_root(dz * dz + dx * dx);
    float distance = float(metric);
    if (static_cast<long double>(std::bit_cast<float>(0x3727C5ACu)) > metric)
    {
        distance = radius - std::bit_cast<float>(0x3C23D70Au);
    }
    if (!(radius > distance)) // JBE rejects unordered too.
    {
        return false;
    }
    Vec3 direction;
    runtime_actor_planar_delta(first, second, direction);
    if (query != nullptr)
    {
        const float x = query->x * -1.f;
        const float z = query->z * -1.f;
        const float dot = (direction.z * z + x * direction.x) + direction.y * 0.f;
        if (!(0.f > dot))
        {
            direction = {x, 0.f, z};
        }
        else
        {
            direction = {x * -1.f, -0.f, z * -1.f}; // DAT_0032B4D4.
        }
    }
    const Vec3 center{fixed.x, fixed.y, fixed.z};
    sweep_runtime_directed_cylinder_push(point, center, moving.radius, fixed.radius,
        direction, push, normalize);
    const float middle = fixed.height * .5f + fixed.y;
    contact.x = ((offset.x + moving.x) - fixed.x) * .5f + fixed.x;
    contact.y = ((offset.y + (moving.height * .5f + moving.y)) - middle) * .5f + middle;
    contact.z = ((offset.z + moving.z) - fixed.z) * .5f + fixed.z;
    return true;
}

// Xbox 0x0003C470
bool push_runtime_attachment_projection(const Actor& first, const Actor& second,
    const EntityCylinderBound& candidate, Vec3& output)
{
    Vec3 direction;
    runtime_actor_planar_delta(first, second, direction);
    const float squared = direction.z * direction.z + direction.x * direction.x;
    if (squared != 0.f)
    {
        const long double reciprocal = retail_reciprocal_sqrt(squared);
        direction.x = float(static_cast<long double>(direction.x) * reciprocal);
        direction.y = float(0.L * reciprocal);
        direction.z = float(static_cast<long double>(direction.z) * reciprocal);
    }
    const Vec3 position = actor_world_position(first);
    const float projection = ((candidate.z * direction.z + candidate.y * direction.y)
        + candidate.x * direction.x) - ((position.z * direction.z + position.y * direction.y)
        + direction.x * position.x);
    if (candidate.radius > projection) // JBE rejects unordered.
    {
        const float distance = candidate.radius - projection;
        output.x = direction.x * distance + output.x;
        output.y = direction.y * distance + output.y;
        output.z = direction.z * distance + output.z;
        return true;
    }
    return false;
}

// Xbox 0x0003C3A0
bool accumulate_runtime_actor_shape_push(const Actor& first, const Actor* second,
    const Vec3* query, const EntityCylinderBound& fixed, const EntityCylinderBound& merged,
    EntityCylinderBound* shapes, i32 count, bool attached, Vec3& output,
    RuntimeActorShapePush& context)
{
    if (!runtime_offset_cylinders_apart(fixed, merged, output))
    {
        const auto end = reinterpret_cast<std::uintptr_t>(shapes)
            + std::bit_cast<i32>(u32(count) * 0x20u);
        for (auto* shape = shapes; reinterpret_cast<std::uintptr_t>(shape) < end; ++shape)
        {
            Vec3 push;
            const bool hit = attached
                ? push_runtime_attached_cylinder(first, *second, fixed, *shape, query,
                    output, push, context.contact, context.normalize)
                : push_runtime_collision_cylinder(*shape, fixed, output, push, context.contact);
            if (hit)
            {
                output.x = output.x + push.x;
                output.y = output.y + push.y;
                output.z = output.z + push.z;
                return true;
            }
        }
    }
    return false;
}

// Xbox 0x0003C5D0
void notify_runtime_actor_shape_push(const Actor* other, const Vec3& output,
    u16 bone, RuntimeActorShapePush& context)
{
    if (other != nullptr)
    {
        context.message(*other, output, context.contact, bone);
    }
}

// Xbox 0x0003C0F0
bool push_runtime_actor_shapes(const Actor* other, const Vec3* query,
    const EntityCylinderBound& merged, EntityCylinderBound* shapes, i32 count,
    Vec3& output, RuntimeActorShapePush& context)
{
    Actor& actor = context.actor;
    if (actor.model != nullptr && actor.model->cpu.data() != nullptr)
    {
        const auto cpu = actor.model->cpu;
        const u32 table = read_u32(cpu, 0);
        const u32 rows = read_u32(cpu, 4);
        for (u32 index = 0; index < rows; ++index)
        {
            if (read_u32(cpu, table + index * 8) == 4)
            {
                const u32 block = read_u32(cpu, table + index * 8 + 4);
                if (block == 0)
                {
                    return false;
                }
                Vec3 push{};
                const bool attached = other != nullptr && other->attach == &actor;
                bool hit = false;
                u16 bone = 0;
                if (attached)
                {
                    hit = push_runtime_attachment_projection(actor, *other, merged, push);
                }
                else if (other != nullptr && actor.shape_mode < other->shape_mode)
                {
                    return false;
                }
                Vec3 position;
                float height, radius;
                runtime_actor_cylinder_metrics(actor, position, height, radius);
                EntityCylinderBound bound;
                bound.kind = 4;
                bound.size = 0x20;
                bound.flag = 1;
                bound.x = position.x;
                bound.y = position.y;
                bound.z = position.z;
                bound.pad0 = 0.f;
                bound.height = height;
                bound.pad1 = 0.f;
                bound.radius = radius;
                if (!runtime_cylinders_apart(bound, merged))
                {
                    const u32 mask = u32(actor.model_mode);
                    const EntityBonePose* pose = actor.model->alternate_shape_pose;
                    if (pose == nullptr)
                    {
                        pose = actor.model->primary_shape_pose;
                    }
                    const u8* node = cpu.data() + block + 8;
                    while (node != nullptr)
                    {
                        const i16 id = std::bit_cast<i16>(read_u16({node, 4}, 2));
                        const u32 word = actor.model != nullptr ? context.model_words[id] : 0xFFFFFFFFu;
                        if ((mask & read_u32({node, 8}, 4) & word) != 0)
                        {
                            bool supported = true;
                            if (node[0] == 0)
                            {
                                transform_runtime_shape_sphere(*reinterpret_cast<const EntityShapeNode*>(node),
                                    actor.xform, pose, bound);
                            }
                            else if (node[0] == 1)
                            {
                                transform_runtime_shape_cylinder(*reinterpret_cast<const EntityShapeNode*>(node),
                                    actor.xform, pose, reinterpret_cast<u8*>(&bound));
                            }
                            else
                            {
                                supported = false;
                            }
                            if (supported && accumulate_runtime_actor_shape_push(actor, other, query,
                                bound, merged, shapes, count, attached, push, context))
                            {
                                bone = std::bit_cast<u16>(id);
                                hit = true;
                            }
                        }
                        node = node[1] != 0 ? node + node[1] : nullptr;
                    }
                }
                if (!hit)
                {
                    return false;
                }
                output = push;
                notify_runtime_actor_shape_push(other, output, bone, context);
                return true;
            }
        }
    }
    return false;
}
}
