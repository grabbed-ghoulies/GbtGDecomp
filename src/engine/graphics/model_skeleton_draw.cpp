#include "model_skeleton_draw.h"

namespace gbtg
{
// Xbox 0x001569D0
void dispatch_runtime_model_skeleton(u32 node, void* storage,
    RuntimeModelSkeletonDraw& context)
{
    const u32 offset = context.tree.image.word(node, 0xC);
    auto& state = runtime_render_state(context.tree.renderer);
    state.skeleton_pose = context.pose(storage, offset + 4);
    state.skeleton_matrices = context.tree.image.word(node, 0x24);
    state.skeleton_joint_count = context.tree.image.word(node, 0x20);
    queue_runtime_model_group(node, context.tree);
}
}
