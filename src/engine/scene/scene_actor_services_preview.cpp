// The inspector's asset viewer model (docs/INSPECTOR.md, "Asset viewer"). Every step is a native one: the resource
// provider loads the AID, 0x000F9840 instances it, the animation controller poses it and 0x000F9080 draws it into the
// recording device. Only the choice of model, animation and camera is the inspector's.
#if defined(GBTG_INSPECTOR_APP)
#include "scene_actor_services_state.h"
#include "../graphics/actor_effect_draw.h"
#include "../scripts/script_vm.h"

#include <algorithm>
#include <cstring>

namespace gbtg
{
namespace
{
const AnimationMatrix kIdentity{1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f};

void release_preview_animation(SceneModelPreview& preview, AnimationResourceRegistry& registry)
{
    if (preview.animation.controller != nullptr)
    {
        release_animation_controller(preview.animation.controller, false, registry); // 0x000307C0
    }
    if (preview.animation_resource.data() != nullptr)
    {
        release_animation_resource(registry, preview.animation_resource); // 0x00035C00
    }
    preview.animation_resource = {};
    preview.animation_aid.clear();
    if (preview.pose.primary != nullptr)
    {
        initialize_model_pose(*preview.pose.primary); // 0x000314C0: the bind pose.
    }
}
}

void SceneActorServices::release_preview()
{
    auto& state = *state_;
    if (state.model_preview == nullptr)
    {
        return;
    }
    SceneModelPreview& preview = *state.model_preview;
    ActorEffectModelContext& create = state.effect_models;
    release_preview_animation(preview, create.registry);
    if (preview.model != nullptr)
    {
        state.flag_origins.erase(preview.model.get());
        // As 0x0012A9B0 releases a post model.
        const RuntimeModelAsset* old = defer_runtime_model_release(*preview.model, create.deferred, create.fence,
            create.registry, create.shaders, create.heap_free); // 0x000F8BF0
        release_animation_resource(create.registry, old->cpu); // 0x00035C00
        create.deferred.records.back().owner = std::move(preview.model);
    }
    state.model_preview.reset();
}

void SceneActorServices::preview_model(const std::string& aid)
{
    release_preview();
    if (aid.empty())
    {
        return;
    }
    auto& state = *state_;
    ActorEffectModelContext& create = state.effect_models;
    const std::span<const u8> resource = acquire_animation_resource(create.registry, aid.c_str()); // 0x00035E90
    if (resource.data() == nullptr)
    {
        throw Error("the native resource provider has no " + aid);
    }
    const auto asset = create.views.assets.find(resource.data());
    if (asset == create.views.assets.end() || asset->second == nullptr)
    {
        release_animation_resource(create.registry, resource);
        throw Error(aid + " is not a relocated native model resource");
    }
    auto preview = std::make_unique<SceneModelPreview>();
    preview->model_aid = aid;
    preview->model_resource = resource;
    try
    {
        preview->model = make_runtime_model_instance(*asset->second, 1, 1, create.types, create.shaders,
            create.scratch); // 0x000F9840, as 0x0012A9E0 makes a post model
    }
    catch (...)
    {
        release_animation_resource(create.registry, resource);
        throw;
    }
    // The pose view an actor's model gives the controller (refresh_actor_model_animation_view).
    RuntimeModelInstance& model = *preview->model;
    preview->pose.primary = model.primary;
    preview->pose.alternate = model.alternate;
    if (model.skeleton_node != nullptr)
    {
        preview->pose.skeleton = model.skeleton_node->joints;
        preview->pose.joint_count = int(model.skeleton_node->joints.size());
    }
    preview->pose.auxiliary_targets = model.auxiliary_targets;
    std::copy(model.node_flags.begin(), model.node_flags.end(), preview->pose.node_flags.begin());
    for (const RuntimeModelResourceBlock& block : model.source->blocks)
    {
        preview->pose_blocks.push_back(ModelResourceBlock{block.type, block.word});
    }
    preview->pose.resource_blocks = preview->pose_blocks;
    state.model_preview = std::move(preview);
}

void SceneActorServices::preview_animation(const std::string& aid)
{
    auto& state = *state_;
    if (state.model_preview == nullptr)
    {
        throw Error("choose a model before an animation");
    }
    SceneModelPreview& preview = *state.model_preview;
    ActorEffectModelContext& create = state.effect_models;
    release_preview_animation(preview, create.registry);
    if (aid.empty())
    {
        return;
    }
    const std::span<const u8> resource = acquire_animation_resource(create.registry, aid.c_str()); // 0x00035E90
    if (resource.data() == nullptr)
    {
        throw Error("the native resource provider has no " + aid);
    }
    // The controller samples one pose per MINA bone (+0x14) and writes one matrix per skeleton joint.
    const u32 bones = resource.size() >= 0x16 ? read_u16(resource, 0x14) : 0;
    if (bones != u32(preview.pose.joint_count))
    {
        release_animation_resource(create.registry, resource);
        throw Error(aid + " has " + std::to_string(bones) + " bones; " + preview.model_aid + "'s skeleton has "
            + std::to_string(preview.pose.joint_count) + " joints");
    }
    preview.animation_aid = aid;
    preview.animation_resource = resource;
    preview.animation.controller = make_animation_controller(1); // 0x00030740
    AnimationMix mix; // 0x000322A0's defaults for a looping record with no events.
    if (!start_controller_animation(*preview.animation.controller, 0, resource, nullptr, mix, create.registry, create.error,
        preview.random_state)) // 0x00030880
    {
        release_preview_animation(preview, create.registry);
        throw Error("0x00030880 did not start " + aid);
    }
    tick_preview(0.f, false);
}

void SceneActorServices::tick_preview(float dt, bool paused)
{
    auto& state = *state_;
    if (state.model_preview == nullptr || state.model_preview->animation.controller == nullptr)
    {
        return;
    }
    SceneModelPreview& preview = *state.model_preview;
    AnimationController& controller = *preview.animation.controller;
    if (controller.slots[0].player != nullptr)
    {
        controller.slots[0].player->paused = paused ? 1 : 0;
    }
    tick_animation_controller(controller, dt, false, state.effect_models.registry); // 0x00030BA0
    apply_controller_to_model(controller, preview.pose, false, nullptr, preview.flag_cache, kIdentity); // 0x00030C70
    // The pose view's node flags back to the model (+0x294), as an actor's view stores them; the preview owns it.
    RuntimeModelInstance& model = *preview.model;
    std::copy_n(preview.pose.node_flags.begin(), model.node_flags.size(), model.node_flags.begin());
}

void SceneActorServices::seek_preview(float position)
{
    auto& state = *state_;
    if (state.model_preview == nullptr || state.model_preview->animation.controller == nullptr)
    {
        return;
    }
    set_actor_animation_position(state.model_preview->animation, 0, position); // 0x0003B2E0
    tick_preview(0.f, true);
}

void SceneActorServices::inspect_preview(const InspectionView& camera, bool full_light, InspectionFrame& frame)
{
    auto& state = *state_;
    if (state.model_preview == nullptr || state.model_preview->model == nullptr)
    {
        throw Error("the asset viewer has no model");
    }
    if (state.display_setup == nullptr || state.model_draw == nullptr)
    {
        throw Error("0x000F9080 draws before the scene's display views exist");
    }
    inspect_draw(camera, frame, "asset viewer model draw 0x000F9080", [this, full_light]()
    {
        auto& state = *state_;
        RuntimeModelInstance& model = *state.model_preview->model;
        state.flag_origins[&model] = model.node_flags.data(); // Model +0x294, as every model draw registers it.
        std::array<float, 4> tint{g_scene_ambient_tint_0, g_scene_ambient_tint_1, g_scene_ambient_tint_2,
            g_scene_ambient_tint_3};
        if (full_light)
        {
            // 0x0012AB10 with the post model's +0x28 set.
            g_scene_ambient_tint_3 = 1.f;
            g_scene_ambient_tint_0 = 1.f;
            g_scene_ambient_tint_1 = 1.f;
            g_scene_ambient_tint_2 = 1.f;
        }
        try
        {
            // The passes an actor's model gets from the view (0x00121900 kinds 0 and 1, 0x00121D40 kind 2), each
            // gated as 0x0003C9E0 gates it by the model header's byte 8, at the origin with no rotation.
            SceneModelDrawContext& draw = *state.model_draw;
            const u32 cpu = state.resources.memory.address_of(model.source->cpu.data());
            const u8 passes = draw.image.byte(cpu, 8);
            const AnimationMatrix world{1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f};
            if ((passes & 0x10) != 0)
            {
                draw_runtime_scene_model(model, cpu, world, 0, draw); // 0x000F9080
            }
            draw_runtime_scene_model(model, cpu, world, 1, draw);
            if ((passes & 8) != 0)
            {
                draw_runtime_scene_model(model, cpu, world, 2, draw);
            }
        }
        catch (...)
        {
            g_scene_ambient_tint_0 = tint[0];
            g_scene_ambient_tint_1 = tint[1];
            g_scene_ambient_tint_2 = tint[2];
            g_scene_ambient_tint_3 = tint[3];
            throw;
        }
        g_scene_ambient_tint_0 = tint[0];
        g_scene_ambient_tint_1 = tint[1];
        g_scene_ambient_tint_2 = tint[2];
        g_scene_ambient_tint_3 = tint[3];
    });
}

SceneModelPreviewInfo SceneActorServices::preview_info() const
{
    SceneModelPreviewInfo info;
    const auto& state = *state_;
    if (state.model_preview == nullptr)
    {
        return info;
    }
    const SceneModelPreview& preview = *state.model_preview;
    info.model = preview.model_aid;
    info.animation = preview.animation_aid;
    info.joints = u32(preview.pose.joint_count);
    if (preview.animation_resource.size() >= 0x16)
    {
        info.animation_bones = read_u16(preview.animation_resource, 0x14);
    }
    if (preview.animation.controller != nullptr && preview.animation.controller->slots[0].player != nullptr)
    {
        const MinaPlayer& player = *preview.animation.controller->slots[0].player;
        info.position = player.position;
        info.duration = player.duration;
        info.paused = player.paused != 0;
    }
    const auto walk = [&info](auto& self, const RuntimeModelNode* node) -> void
    {
        for (; node != nullptr; node = node->sibling)
        {
            // Only ndBGPushBuffer holds bounds there. 0x00157140 tests just the third letter, 'B', because it walks
            // a background shader's subtree; a whole tree also has ndBlendshape nodes, with other data at +0x48.
            if (node->type_name != nullptr && std::strcmp(node->type_name, "ndBGPushBuffer") == 0
                && node->bytes.size() >= 0x60)
            {
                Vec3 low;
                Vec3 high;
                std::memcpy(&low, node->bytes.data() + 0x48, sizeof(low));
                std::memcpy(&high, node->bytes.data() + 0x54, sizeof(high));
                if (!info.has_bounds)
                {
                    info.bounds_min = low;
                    info.bounds_max = high;
                    info.has_bounds = true;
                }
                info.bounds_min = {std::min(info.bounds_min.x, low.x), std::min(info.bounds_min.y, low.y),
                    std::min(info.bounds_min.z, low.z)};
                info.bounds_max = {std::max(info.bounds_max.x, high.x), std::max(info.bounds_max.y, high.y),
                    std::max(info.bounds_max.z, high.z)};
            }
            self(self, node->child);
        }
    };
    if (preview.model != nullptr && preview.model->source != nullptr && preview.model->source->tree != nullptr)
    {
        for (const RuntimeModelNode* root : preview.model->source->tree->roots)
        {
            walk(walk, root);
        }
    }
    const ModelPoseBuffer* pose = preview.pose.alternate != nullptr ? preview.pose.alternate : preview.pose.primary;
    if (pose != nullptr && pose->count > 0)
    {
        for (int joint = 0; joint < preview.pose.joint_count && joint < pose->count; ++joint)
        {
            const Joint& record = preview.pose.skeleton[size_t(joint)];
            const AnimationMatrix& world = model_joint_world_matrix(*pose, joint, record, kIdentity); // 0x000316B0
            info.joint_points.push_back({world[3], world[7], world[11]});
            info.joint_parents.push_back(record.parent);
        }
    }
    return info;
}
}
#endif
