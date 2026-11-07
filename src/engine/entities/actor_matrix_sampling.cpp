#include "actor_matrix_sampling.h"
#include "actor_matrix_setup.h"
#include "actor_pose_updates.h"
#include "actor_transform.h"
#include "gbtg/model_runtime.h"

#include <bit>
#include <cstring>

namespace gbtg
{
// Xbox 0x00040640
void sample_actor_model_matrix(Actor& actor, i32 index, Vec3& position, Vec3& euler,
    i32 clamp_height, RuntimeActorMatrixSampling& context)
{
    ActorMatrixRecord& record = context.records[size_t(index)];
    if (record.active != 0)
    {
        position = record.translation;
        euler = record.euler;
        return;
    }
    context.active = context.scratch.data();
    std::memcpy(context.active, actor.xform, 48);
    const i32 joint = std::bit_cast<i32>(record.binding);
    if (joint != -1)
    {
        const ModelPoseBuffer* pose = nullptr;
        if (actor.model != nullptr)
        {
            pose = actor.model->runtime->alternate;
            if (pose == nullptr)
            {
                pose = actor.model->runtime->primary;
            }
        }
        const float* bone = nullptr;
        if (pose != nullptr)
        {
            bone = model_pose_matrix(*pose, joint).data();
        }
        multiply_affine_matrices(context.active, context.active, bone);
    }
    const auto matrix = context.resolve(runtime_matrix_array_matrix_offset(context.node, index));
    const AnimationMatrix local{read_f32(matrix, 0), read_f32(matrix, 0x10), read_f32(matrix, 0x20), read_f32(matrix, 0x30),
        read_f32(matrix, 4), read_f32(matrix, 0x14), read_f32(matrix, 0x24), read_f32(matrix, 0x34),
        read_f32(matrix, 8), read_f32(matrix, 0x18), read_f32(matrix, 0x28), read_f32(matrix, 0x38)};
    multiply_affine_matrices(context.active, context.active, local.data());
    AnimationMatrix copied;
    std::memcpy(copied.data(), context.active, 48);
    const float* world = context.active;
    const float x = ((world[1] * context.origin.y + world[2] * context.origin.z) + world[0] * context.origin.x) + world[3];
    const float y = ((world[5] * context.origin.y + world[6] * context.origin.z) + world[4] * context.origin.x) + world[7];
    const float z = ((world[9] * context.origin.y + world[10] * context.origin.z) + world[8] * context.origin.x) + world[11];
    position.x = x;
    position.y = y;
    position.z = z;
    matrix_axis_scales(copied.data(), record.scale);
    euler = actor_matrix_euler(copied.data());
    if (clamp_height != 0)
    {
        const Vec3 actor_position = actor_world_position(actor);
        if (actor_position.y > position.y)
        {
            position.y = actor_position.y;
            record.translation.y = actor_position.y;
        }
    }
}
}
