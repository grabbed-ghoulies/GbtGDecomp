#pragma once

#include "gbtg/entity.h"
#include "gbtg/anim.h"

namespace gbtg
{
const AnimationMatrix* actor_pose_joint_matrix(const Actor& actor, i32 joint); // Xbox 0x00040B40
void matrix_axis_scales(const float* matrix, Vec3& output); // Xbox 0x00103A80
void update_actor_model_factor(Actor& actor); // Xbox 0x0003D730
void set_runtime_actor_body_position(Actor& actor, const Vec3& position); // Xbox 0x0002B430
void set_runtime_actor_body_euler(Actor& actor, const Vec3& euler); // Xbox 0x0002B560
void update_actor_animation_pose(Actor& actor, ActorAnimationState& animation,
    ModelAnimationState& model, u16 bone, AnimationResourceRegistry& registry,
    const SkeletonDispatchContext* callbacks, ModelNodeFlagCache& cache,
    const AnimationMatrix& reset); // Xbox 0x0003AE00
}
