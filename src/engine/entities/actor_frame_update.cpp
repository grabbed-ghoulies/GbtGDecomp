#include "actor_frame_update.h"
#include <bit>

namespace gbtg
{
// Xbox 0x000420D0
void update_actor_frame(ActorFrameState& state, ActorFrameContext& context)
{
    Actor& actor = state.actor;
    float rate = 0.f;
    float step;
    if (context.freeze <= 0.f)
    {
        step = context.dt * context.scene_scale;
    }
    else
    {
        step = 0.f;
    }
    if (actor.animation_pause <= 0.f)
    {
        rate = actor.animation_rate;
    }
    if (state.animation.controller != nullptr)
    {
        tick_animation_controller(*state.animation.controller, rate * step,
            state.bone == 0, context.effects.models.registry);
    }
    if (state.animation.controller != nullptr)
    {
        apply_controller_to_model(*state.animation.controller, state.model,
            state.bone == 0, context.pose_callbacks, context.node_cache, context.reset_matrix);
    }
    ActorFrameAttachment* update = state.updates;
    while (update != nullptr)
    {
        ActorFrameAttachment* next = update->next;
        update->update(*update, actor);
        update = next;
    }
    update_actor_attachment_effects(actor, state.attachments, state.pending, context.effects);
    for (auto* anchor = static_cast<ActorModelAnchor*>(state.anchors.next);
        anchor != nullptr; anchor = static_cast<ActorModelAnchor*>(anchor->owner_link.next))
    {
        update_actor_model_anchor(*anchor);
    }
    float z = state.target.z;
    float y = state.target.y;
    float x = state.target.x;
    if (context.ease != 0)
    {
        constexpr float fraction = std::bit_cast<float>(0x3DCCCCCDu); // DAT_0032B254
        x = (x - state.eased.x) * fraction + state.eased.x;
        y = (y - state.eased.y) * fraction + state.eased.y;
        z = (z - state.eased.z) * fraction + state.eased.z;
    }
    state.eased.x = x;
    state.eased.y = y;
    state.eased.z = z;
    if (actor.model != nullptr && actor.model->runtime->shader_animations != nullptr)
    {
        tick_runtime_model_shader_animations(*actor.model->runtime->shader_animations,
            1.f, context.shader_matrices);
    }
    if (context.block_decay == 0 && context.decay_enabled != 0)
    {
        if (context.freeze <= 0.f)
        {
            step = context.dt * context.scene_scale;
        }
        else
        {
            step = 0.f;
        }
        if (0.f < actor.animation_pause)
        {
            step = actor.animation_pause - step;
            actor.animation_pause = step;
            if (step <= 0.f)
            {
                actor.animation_pause = 0.f;
            }
        }
    }
    rate = 0.f;
    if (context.freeze <= 0.f)
    {
        step = context.dt * context.scene_scale;
    }
    else
    {
        step = 0.f;
    }
    if (actor.animation_pause <= 0.f)
    {
        rate = actor.animation_rate;
    }
    state.elapsed = rate * step + state.elapsed;
    ++state.ticks;
}
}
