#pragma once
#include "actor_attachment_requests.h"
#include "actor_model_anchors.h"

namespace gbtg
{
struct ActorFrameAttachment
{
    ActorFrameAttachment* next; // embedded link +4, actor list +0x1D8
    void (*update)(ActorFrameAttachment&, Actor&); // object vtable +0x10
};
struct ActorFrameState
{
    Actor& actor;
    ActorAnimationState& animation;
    ModelAnimationState& model;
    ActorAttachmentList& attachments; // actor +0x1AC
    ActorAttachmentNode& pending; // actor +0x1B4
    ActorFrameAttachment* updates; // actor +0x1D8
    entity_detail::ListHead& anchors; // actor +0x1F4
    u16 bone; // actor +0x194
    Vec3 eased; // actor +0x318
    Vec3 target; // actor +0x324
    float elapsed; // actor +0x924
    u32 ticks; // actor +0x928
};
struct ActorFrameContext
{
    ActorEffectStreamContext& effects;
    const SkeletonDispatchContext* pose_callbacks;
    ModelNodeFlagCache& node_cache;
    const AnimationMatrix& reset_matrix;
    const RuntimeShaderMatrixBoundaries& shader_matrices;
    float freeze; // DAT_004312DC
    float dt; // DAT_005096C0
    float scene_scale; // DAT_004312C4
    u32 ease; // DAT_00509B74
    u32 block_decay; // DAT_0050E768
    u8 decay_enabled; // DAT_0030F144[DAT_0051248C]
};
void update_actor_frame(ActorFrameState& state, ActorFrameContext& context); // Xbox 0x000420D0
}
