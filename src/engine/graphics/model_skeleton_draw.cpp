#include "model_skeleton_draw.h"

namespace gbtg
{
// Xbox 0x001569D0
void dispatch_runtime_model_skeleton(u32 node, void* storage,
    RuntimeModelSkeletonDraw& context)
{
    const u32 offset = context.tree.image.get(node, model_node_field::instance_offset);
    auto& state = runtime_render_state(context.tree.renderer);
    state.skeleton_pose = context.pose(storage, offset + 4);
    state.skeleton_matrices = context.tree.image.get(node, skeleton_node::joints);
    state.skeleton_joint_count = context.tree.image.get(node, skeleton_node::joint_count);
    queue_runtime_model_group(node, context.tree);
}
}
