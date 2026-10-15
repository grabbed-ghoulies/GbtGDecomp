#include "actor_frame_render.h"
#include "../graphics/scene_render_helpers.h"
#include <cstring>

namespace gbtg
{
// Xbox 0x0003C9E0
u32 draw_runtime_actor(ActorDrawState& state, i32 kind,
    const ActorFrameDrawContext& attachments, SceneModelDrawContext& c)
{
    if (state.hidden != 0)
    {
        return 0;
    }
    draw_actor_frame_attachments(state.frame, kind, attachments);
    ActorDrawModelView* model = state.model;
    if (model == nullptr)
    {
        return 0;
    }
    if (kind == 0)
    {
        if ((c.image.byte(model->cpu, 8) & 0x10) == 0)
        {
            return 0;
        }
    }
    else if (kind == 2 && (c.image.byte(model->cpu, 8) & 8) == 0)
    {
        return 0;
    }
    Actor& actor = state.frame.actor;
    if (state.matrices_ready == 0)
    {
        refresh_actor_model_matrices(actor, state.matrices, 0, c.matrices);
    }
    if (!equal_runtime_packed_colors(state.material_second, 0xFF000000))
    {
        model->material_enabled = 1;
        model->material_first = state.material_first;
        model->material_second = state.material_second;
    }
    else
    {
        model->material_enabled = 0;
    }
    if (state.before)
    {
        state.before(actor, kind, state.payload);
    }
    AnimationMatrix world;
    std::memcpy(world.data(), actor.xform, sizeof(actor.xform));
    draw_runtime_scene_model(model->instance, model->cpu, world, static_cast<u32>(kind), c);
    if (state.after)
    {
        state.after(actor, kind, state.payload);
    }
    return 1;
}
// Xbox 0x00042C10
u32 draw_runtime_background(i32 dialogue_mode, ActorDrawState& state, i32 kind,
    const ActorFrameDrawContext& attachments, SceneModelDrawContext& c)
{
    if (dialogue_mode != 0)
    {
        return 0;
    }
    return draw_runtime_actor(state, kind, attachments, c);
}
// Xbox 0x0003DE00
void draw_actor_frame_attachments(ActorFrameState& state, i32 kind,
    const ActorFrameDrawContext& context)
{
    ActorFrameAttachment* attachment = state.updates;
    while (attachment != nullptr)
    {
        ActorFrameAttachment* next = attachment->next;
        context.draws.at(attachment)(*attachment, state.actor, kind);
        attachment = next;
    }
}
}
