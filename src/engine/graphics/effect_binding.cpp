#include "effect_binding.h"
#include "../entities/model_binding_selection.h"
#include "../entities/actor_model_anchors.h"
#include "../entities/actor_sound_pose.h"
#include "../entities/shape_transform.h"
#include <cstring>

namespace gbtg
{
// Xbox 0x000EC830
void sample_actor_effect_shape(const Actor* actor, u32 mask, const u8*& cached,
    Vec3& position, float& radius)
{
    if (actor == nullptr)
    {
        position = {0.f, 0.f, 0.f};
        return;
    }
    const u8* shape = select_actor_effect_shape(*actor, cached, mask);
    if (shape != nullptr)
    {
        cached = shape;
        const EntityBonePose* pose = nullptr;
        if (actor->model != nullptr)
        {
            pose = actor->model->alternate_shape_pose;
            if (pose == nullptr)
            {
                pose = actor->model->primary_shape_pose;
            }
        }
        radius = shape_sphere_metrics(*reinterpret_cast<const EntityShapeNode*>(shape),
            actor->xform, pose, position);
        return;
    }
    position = actor_world_position(*actor);
}

// Xbox 0x000EC8E0
void sample_actor_effect_locator(const Actor* actor, i16 kind, const u8*& cached,
    Vec3& position, Vec3& euler)
{
    if (actor == nullptr)
    {
        position = {0.f, 0.f, 0.f};
        return;
    }
    const u8* locator;
    if (cached == nullptr || !advance_actor_model_binding(*actor, cached, kind))
    {
        locator = find_actor_model_binding(*actor, kind);
    }
    else
    {
        locator = cached;
    }
    if (locator == nullptr)
    {
        position = actor_world_position(*actor);
        return;
    }
    ModelBoneBinding binding;
    static_assert(sizeof(binding) == 0x34);
    std::memcpy(&binding, locator, 0x34);
    actor_binding_world_position(*actor, binding, position);
    actor_binding_world_euler(*actor, binding, euler);
    cached = locator;
}

// Xbox 0x000EC9D0
void sample_actor_effect_joint(const Actor* actor, i32 joint, Vec3& position, Vec3& euler)
{
    if (actor != nullptr && joint != -1)
    {
        actor_transform_joint_point(*actor, u16(joint), actor_transform_origin, position);
        actor_joint_world_euler(*actor, u16(joint), euler);
        return;
    }
    position = actor != nullptr ? actor_world_position(*actor) : Vec3{0.f, 0.f, 0.f};
}

// Xbox 0x000EEA00
void update_actor_effect_size(ActorEffectNode& effect, const std::function<u32(const u8*)>& address)
{
    const u32 kind = get(effect.bytes, actor_effect_field::volume);
    if (kind == 0)
    {
        Vec3 position = get(effect.bytes, actor_effect_field::emit_point);
        float radius = get(effect.bytes, actor_effect_field::volume_size).x;
        sample_actor_effect_shape(static_cast<const Actor*>(effect.binding_actor),
            get(effect.bytes, actor_effect_field::binding_index), effect.binding_shape, position, radius);
        set(effect.bytes, actor_effect_field::emit_point, position);
        field_data(effect.bytes, actor_effect_field::volume_size)->x = radius;
        set(effect.bytes, actor_effect_field::binding_shape, address(effect.binding_shape));
    }
    else if (kind == 5)
    {
        Vec3 position = get(effect.bytes, actor_effect_field::emit_point);
        float radius = get(effect.bytes, actor_effect_field::volume_size).x;
        sample_actor_effect_shape(static_cast<const Actor*>(effect.binding_actor),
            get(effect.bytes, actor_effect_field::binding_index), effect.binding_shape, position, radius);
        set(effect.bytes, actor_effect_field::emit_point, position);
        field_data(effect.bytes, actor_effect_field::volume_size)->x = radius;
        set(effect.bytes, actor_effect_field::binding_shape, address(effect.binding_shape));
        field_data(effect.bytes, actor_effect_field::volume_size)->y =
            get(effect.bytes, actor_effect_field::volume_size).x;
        field_data(effect.bytes, actor_effect_field::volume_size)->z =
            get(effect.bytes, actor_effect_field::volume_size).x;
    }
}

// Xbox 0x000EEB60
void prepare_actor_effect_binding(ActorEffectNode& effect, float step,
    const std::function<u32(const u8*)>& address)
{
    const Vec3 previous = get(effect.bytes, actor_effect_field::emit_point);
    const Actor* actor = static_cast<const Actor*>(effect.binding_actor);
    if (actor == nullptr)
    {
        Vec3 local;
        rotate_euler_vector(get(effect.bytes, actor_effect_field::spin), get(effect.bytes, actor_effect_field::offset),
            local);
        const Vec3 origin = get(effect.bytes, actor_effect_field::position);
        set(effect.bytes, actor_effect_field::emit_point, {origin.x + local.x, origin.y + local.y, origin.z + local.z});
    }
    else
    {
        const u32 mode = get(effect.bytes, actor_effect_field::binding_mode);
        if (mode == 0 || mode == 1 || mode == 2)
        {
            set(effect.bytes, actor_effect_field::position, actor_world_position(*actor));
            set(effect.bytes, actor_effect_field::euler, actor_world_euler(*actor));
            if (mode == 0)
            {
                update_actor_effect_size(effect, address);
            }
            else
            {
                Vec3 position = get(effect.bytes, actor_effect_field::emit_point);
                Vec3 euler = get(effect.bytes, actor_effect_field::euler);
                if (mode == 1)
                {
                    sample_actor_effect_locator(actor, i16(get(effect.bytes, actor_effect_field::binding_locator)),
                        effect.binding_locator, position, euler);
                    set(effect.bytes, actor_effect_field::binding_locator_address, address(effect.binding_locator));
                }
                else
                {
                    sample_actor_effect_joint(actor, effect.binding_joint, position, euler);
                }
                set(effect.bytes, actor_effect_field::emit_point, position);
                set(effect.bytes, actor_effect_field::euler, euler);
            }
            const Vec3 position = get(effect.bytes, actor_effect_field::emit_point);
            const Vec3 local = get(effect.bytes, actor_effect_field::offset);
            set(effect.bytes, actor_effect_field::emit_point,
                {local.x + position.x, local.y + position.y, local.z + position.z});
        }
    }
    if (step != 0.f && get(effect.bytes, actor_effect_field::ticks) >= 2)
    {
        const Vec3 position = get(effect.bytes, actor_effect_field::emit_point);
        Vec3 velocity{position.x - previous.x, position.y - previous.y, position.z - previous.z};
        set(effect.bytes, actor_effect_field::motion, velocity);
        const float scale = get(effect.bytes, actor_effect_field::motion_scale) / step;
        velocity.x *= scale;
        velocity.y *= scale;
        velocity.z *= scale;
        set(effect.bytes, actor_effect_field::motion, velocity);
        return;
    }
    set(effect.bytes, actor_effect_field::motion, {0.f, 0.f, 0.f});
}
}
