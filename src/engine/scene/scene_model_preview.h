#pragma once
// The inspector's asset viewer (docs/INSPECTOR.md, "Asset viewer"): one model the native resource provider loads,
// instanced by 0x000F9840 and drawn by 0x000F9080, posed by the native animation controller. Host inspection state,
// never installed by the game.
#if defined(GBTG_INSPECTOR_APP)
#include "gbtg/anim.h"
#include "gbtg/model_runtime.h"

#include <memory>
#include <span>
#include <string>
#include <vector>

namespace gbtg
{
struct SceneModelPreview
{
    std::string model_aid;
    std::span<const u8> model_resource; // 0x00035E90's reference, released with the model.
    std::unique_ptr<RuntimeModelInstance> model;
    std::string animation_aid;
    std::span<const u8> animation_resource;
    ActorAnimationState animation; // Its controller (+0x50) is 0x00030740's with one slot.
    std::vector<ModelResourceBlock> pose_blocks; // The model's resource blocks, as the actor pose view holds them.
    ModelAnimationState pose;
    ModelNodeFlagCache flag_cache{};
    u32 random_state = 0;
};

// What the asset viewer shows about the preview; copied, so it holds no native pointers.
struct SceneModelPreviewInfo
{
    std::string model;
    std::string animation;
    u32 joints = 0;
    u32 animation_bones = 0;
    float position = 0.f; // The animation player's normalized position (+0x14).
    float duration = 0.f; // Its seconds per cycle (+0x20).
    bool paused = false;
    std::vector<Vec3> joint_points; // Each joint's world point under the pose, model at the origin.
    std::vector<int> joint_parents;
    // The union of the background push buffers' bounds (ndBGPushBuffer +0x48 / +0x54, which 0x00157140 culls by);
    // unset for a model without them.
    bool has_bounds = false;
    Vec3 bounds_min{};
    Vec3 bounds_max{};
};
}
#endif
