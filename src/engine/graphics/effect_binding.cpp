#include "effect_binding.h"
#include "../entities/model_binding_selection.h"
#include "../entities/actor_model_anchors.h"
#include "../entities/actor_sound_pose.h"
#include "../entities/shape_transform.h"
#include <cstring>

namespace gbtg
{
namespace
{
Vec3 binding_vector(std::span<const u8> bytes, size_t offset)
{
    return {read_f32(bytes, offset), read_f32(bytes, offset + 4), read_f32(bytes, offset + 8)};
}
void store_binding_vector(std::span<u8> bytes, size_t offset, const Vec3& vector)
{
    std::memcpy(bytes.data() + offset, &vector, 12);
}
void store_binding_word(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}
}

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
    const u32 kind = read_u32(effect.bytes, 0xDC);
    if (kind == 0)
    {
        Vec3 position = binding_vector(effect.bytes, 0x34);
        float radius = read_f32(effect.bytes, 0xE0);
        sample_actor_effect_shape(static_cast<const Actor*>(effect.binding_actor),
            read_u32(effect.bytes, 0xC8), effect.binding_shape, position, radius);
        store_binding_vector(effect.bytes, 0x34, position);
        std::memcpy(effect.bytes.data() + 0xE0, &radius, 4);
        store_binding_word(effect.bytes, 0xCC, address(effect.binding_shape));
    }
    else if (kind == 5)
    {
        Vec3 position = binding_vector(effect.bytes, 0x34);
        float radius = read_f32(effect.bytes, 0xE0);
        sample_actor_effect_shape(static_cast<const Actor*>(effect.binding_actor),
            read_u32(effect.bytes, 0xC8), effect.binding_shape, position, radius);
        store_binding_vector(effect.bytes, 0x34, position);
        std::memcpy(effect.bytes.data() + 0xE0, &radius, 4);
        store_binding_word(effect.bytes, 0xCC, address(effect.binding_shape));
        store_binding_word(effect.bytes, 0xE4, read_u32(effect.bytes, 0xE0));
        store_binding_word(effect.bytes, 0xE8, read_u32(effect.bytes, 0xE0));
    }
}

// Xbox 0x000EEB60
void prepare_actor_effect_binding(ActorEffectNode& effect, float step,
    const std::function<u32(const u8*)>& address)
{
    const Vec3 previous = binding_vector(effect.bytes, 0x34);
    const Actor* actor = static_cast<const Actor*>(effect.binding_actor);
    if (actor == nullptr)
    {
        Vec3 local;
        rotate_euler_vector(binding_vector(effect.bytes, 0x5C), binding_vector(effect.bytes, 0x80), local);
        const Vec3 origin = binding_vector(effect.bytes, 0x1C);
        store_binding_vector(effect.bytes, 0x34, {origin.x + local.x, origin.y + local.y, origin.z + local.z});
    }
    else
    {
        const u32 mode = read_u32(effect.bytes, 0xC4);
        if (mode == 0 || mode == 1 || mode == 2)
        {
            store_binding_vector(effect.bytes, 0x1C, actor_world_position(*actor));
            store_binding_vector(effect.bytes, 0x50, actor_world_euler(*actor));
            if (mode == 0)
            {
                update_actor_effect_size(effect, address);
            }
            else
            {
                Vec3 position = binding_vector(effect.bytes, 0x34);
                Vec3 euler = binding_vector(effect.bytes, 0x50);
                if (mode == 1)
                {
                    sample_actor_effect_locator(actor, i16(read_u16(effect.bytes, 0xD0)),
                        effect.binding_locator, position, euler);
                    store_binding_word(effect.bytes, 0xD4, address(effect.binding_locator));
                }
                else
                {
                    sample_actor_effect_joint(actor, effect.binding_joint, position, euler);
                }
                store_binding_vector(effect.bytes, 0x34, position);
                store_binding_vector(effect.bytes, 0x50, euler);
            }
            const Vec3 position = binding_vector(effect.bytes, 0x34);
            const Vec3 local = binding_vector(effect.bytes, 0x80);
            store_binding_vector(effect.bytes, 0x34, {local.x + position.x, local.y + position.y, local.z + position.z});
        }
    }
    if (step != 0.f && read_u32(effect.bytes, 0x14) >= 2)
    {
        const Vec3 position = binding_vector(effect.bytes, 0x34);
        Vec3 velocity{position.x - previous.x, position.y - previous.y, position.z - previous.z};
        store_binding_vector(effect.bytes, 0x40, velocity);
        const float scale = read_f32(effect.bytes, 0x4C) / step;
        velocity.x *= scale;
        velocity.y *= scale;
        velocity.z *= scale;
        store_binding_vector(effect.bytes, 0x40, velocity);
        return;
    }
    store_binding_vector(effect.bytes, 0x40, {0.f, 0.f, 0.f});
}
}
