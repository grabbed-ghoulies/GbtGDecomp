#include "actor_pose_updates.h"
#include "gbtg/model_runtime.h"
#include "actor_transform.h"

#include <cmath>

namespace gbtg
{
// Xbox 0x0002B430
void set_runtime_actor_body_position(Actor& actor, const Vec3& position)
{
    actor_set_world_position(actor, position);
    ActorEulerOut& body = *actor.euler_out;
    body.position = actor_world_position(*body.actor);
}

// Xbox 0x0002B560
void set_runtime_actor_body_euler(Actor& actor, const Vec3& euler)
{
    normalize_euler_angles(actor.euler, euler);
    actor.xform_valid = 0;
    ActorEulerOut& body = *actor.euler_out;
    body.euler = actor_world_euler(*body.actor);
}

// Xbox 0x00040B40
const AnimationMatrix* actor_pose_joint_matrix(const Actor& actor, i32 joint)
{
    const ModelPoseBuffer* pose;
    if (actor.model == nullptr)
    {
        pose = nullptr;
    }
    else
    {
        const RuntimeModelInstance& model = *actor.model->runtime;
        pose = model.alternate;
        if (pose == nullptr)
        {
            pose = model.primary;
        }
    }
    if (pose == nullptr)
    {
        return nullptr;
    }
    return &model_pose_matrix(*pose, joint);
}

// Xbox 0x00103A80
void matrix_axis_scales(const float* matrix, Vec3& output)
{
    i32 index = 0;
    const float* component = matrix + 8;
    float* axis = &output.x;
    do
    {
        const float squared = component[0] * component[0] + component[-4] * component[-4] +
            component[-8] * component[-8];
        axis[index] = squared;
        if (squared != 0.f)
        {
            axis[index] = float(retail_square_root(squared));
        }
        ++index;
        ++component;
    }
    while (index < 3);
}

// Xbox 0x0003D730
void update_actor_model_factor(Actor& actor)
{
    if (actor.model != nullptr && actor.model->cpu.data() != nullptr)
    {
        const auto cpu = actor.model->cpu;
        u32 index = 0;
        const u32 count = read_u32(cpu, 4);
        if (count != 0)
        {
            const u32 table = read_u32(cpu, 0);
            while (read_u32(cpu, table + index * 8) != 5)
            {
                ++index;
                if (count <= index)
                {
                    return;
                }
            }
            if (read_u32(cpu, table + index * 8 + 4) != 0 && actor.model != nullptr && actor.model->cpu.data() != nullptr)
            {
                const u32 first = actor_lookup_joint_index(actor.model->cpu, 0) & 0xFF;
                if (first != 0xFF && first != 0xFFFFFFFF && actor.model != nullptr && actor.model->cpu.data() != nullptr)
                {
                    const u32 second = actor_lookup_joint_index(actor.model->cpu, 0) & 0xFF;
                    if (second != 0xFF && second != 0xFFFFFFFF)
                    {
                        const AnimationMatrix* matrix = actor_pose_joint_matrix(actor, i32(second));
                        if (matrix != nullptr)
                        {
                            Vec3 scales;
                            matrix_axis_scales(matrix->data(), scales);
                            if (!(scales.y >= scales.z))
                            {
                                scales.y = scales.z;
                            }
                            if (scales.x >= scales.y)
                            {
                                scales.y = scales.x;
                            }
                            actor.model_factor = scales.y;
                        }
                    }
                }
            }
        }
    }
}

// Xbox 0x0003AE00
void update_actor_animation_pose(Actor& actor, ActorAnimationState& animation,
    ModelAnimationState& model, u16 bone, AnimationResourceRegistry& registry,
    const SkeletonDispatchContext* callbacks, ModelNodeFlagCache& cache,
    const AnimationMatrix& reset)
{
    if (animation.controller != nullptr)
    {
        tick_animation_controller(*animation.controller, 0.f, bone == 0, registry);
    }
    if (animation.controller != nullptr)
    {
        apply_controller_to_model(*animation.controller, model, bone == 0, callbacks, cache, reset);
    }
    actor_refresh_transform(actor);
    update_actor_model_factor(actor);
}

}
