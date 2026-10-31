#include "scene_post_models.h"
#include "../camera/display_view.h"
#include "../camera/runtime_display_setup.h"
#include "../entities/euler_wrapping.h"
#include "../graphics/actor_effect_draw.h"
#include "gbtg/anim.h"
#include "gbtg/resource_error.h"

#include <cstring>

namespace gbtg
{
// Xbox 0x0012A9B0
void clear_runtime_post_models(RuntimePostModelContext& context)
{
    ActorEffectModelContext& create = context.create;
    for (RuntimePostModel& post : context.models)
    {
        if (post.model != nullptr)
        {
            const RuntimeModelAsset* old = defer_runtime_model_release(*post.model, create.deferred, create.fence,
                create.registry, create.shaders, create.heap_free); // 0x000F8BF0, ESI the model
            release_animation_resource(create.registry, old->cpu); // 0x00035C00, EBX 0x000F8BF0's result
            create.deferred.records.back().owner = std::move(post.model);
        }
    }
}

// Xbox 0x0012A9E0, EAX name
void load_runtime_post_model(const char* name, u32 index, float scale, RuntimePostModelContext& context)
{
    if (std::memcmp(name, "aid_", 4) != 0)
    {
        return;
    }
    ActorEffectModelContext& create = context.create;
    const std::span<const u8> resource = acquire_animation_resource(create.registry, name); // 0x00035E90
    if (resource.data() == nullptr)
    {
        resource_error(create.error, 2); // 0x00032700
        return;
    }
    RuntimePostModel& post = context.models.at(index);
    post.model = make_runtime_model_instance(*create.views.assets.at(resource.data()), 1, 1, create.types,
        create.shaders, create.scratch); // 0x000F9840, EAX the resource
    post.scale = scale;
    post.spinning = 0.f;
}

// Xbox 0x0012AA50
void set_runtime_post_model(const char* name, RuntimePostModelContext& context)
{
    clear_runtime_post_models(context); // 0x0012A9B0
    load_runtime_post_model(name, 0, 1.f, context); // 0x0012A9E0
}

// Xbox 0x0012AA70
void tick_runtime_post_models(RuntimePostModelContext& context)
{
    if (context.models[0].model == nullptr) // DAT_00512AF0.
    {
        return;
    }
    const float step = runtime_frame_step(context.clock);
    for (RuntimePostModel& post : context.models)
    {
        if (post.spinning != 0.f)
        {
            post.euler.x = post.spin.x * step + post.euler.x;
            post.euler.y = post.spin.y * step + post.euler.y;
            post.euler.z = post.spin.z * step + post.euler.z;
            wrap_runtime_euler_angles(post.euler); // 0x001059B0, ESI +8
        }
    }
}

// Xbox 0x0012AB10
void draw_runtime_post_models(RuntimePostModelContext& context)
{
    if (context.models[0].model == nullptr) // DAT_00512AF0.
    {
        return;
    }
    if (context.display == nullptr || context.draw == nullptr || context.view == nullptr)
    {
        throw Error("0x0012AB10 draws the post models before the scene's display views exist");
    }
    RuntimeDisplaySetupContext& display = *context.display;
    SceneModelDrawContext& draw = *context.draw;
    entity_detail::DisplayView& view = *context.view;
    const Vec3 eye = view.position; // +0.
    const float near_clip = view.near_clip; // +0x2C.
    const float far_clip = view.far_clip; // +0x30.
    view.far_clip = context.far_clip;
    rebuild_display_projection(display); // 0x000F5FD0
    bind_display_projection(display); // 0x000F6170
    draw.lights.environment.upload.renderer.rendering = 1; // DAT_005079E4, cleared by the first draw.
    for (RuntimePostModel& post : context.models)
    {
        if (post.model == nullptr)
        {
            continue;
        }
        std::array<float, 3> tint{};
        if (post.full_light != 0)
        {
            tint = {*context.tint[0], *context.tint[1], *context.tint[2]};
            *context.tint[3] = 1.f; // DAT_0032B0F8.
            *context.tint[0] = 1.f;
            *context.tint[1] = 1.f;
            *context.tint[2] = 1.f;
        }
        const Vec3* position = post.at_origin != 0 ? &context.origin : &eye;
        draw_runtime_model_scaled(*post.model, context.address_of(post.model->source->cpu.data()), position,
            &post.euler, post.scale, 3, display, draw); // 0x000F8CF0, EBX 3
        if (post.full_light != 0)
        {
            *context.tint[3] = 1.f;
            *context.tint[0] = tint[0];
            *context.tint[1] = tint[1];
            *context.tint[2] = tint[2];
        }
    }
    view.near_clip = near_clip;
    view.far_clip = far_clip;
    rebuild_display_projection(display); // 0x000F5FD0
    bind_display_projection(display); // 0x000F6170
}
}
