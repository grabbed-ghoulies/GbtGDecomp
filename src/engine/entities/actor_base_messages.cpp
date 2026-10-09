#include "actor_base_messages.h"
#include "actor_bind_tree.h"
#include "actor_pose_updates.h"
#include "actor_sound_pose.h"

#include <cstring>

namespace gbtg
{
// Xbox 0x0003EF70
void dispatch_base_actor_message(const void* message, ActorBaseContext& context)
{
    u32 id;
    std::memcpy(&id, message, 4);
    RuntimeActorStartupState& state = context.events.state;
    Actor& actor = state.actor;
    auto& models = context.events.effects.models;
    switch (id)
    {
    case 7:
        dispatch_actor_animation_event(*static_cast<const ActorAnimationMessage*>(message), context.events);
        return;
    case 0xB:
    {
        const auto& mix = *static_cast<const ActorAnimationMixMessage*>(message);
        AnimationController* controller = ensure_actor_animation_controller(state.animation);
        start_controller_animation_mix(*controller, mix.slot, mix.mix, mix.name,
            models.registry, models.error, context.events.effects.random_state);
        return;
    }
    case 0xC:
        actor_set_world_position(actor, static_cast<const ActorVectorMessage*>(message)->value);
        return;
    case 0xD:
        normalize_euler_angles(actor.euler, static_cast<const ActorVectorMessage*>(message)->value);
        actor.xform_valid = 0;
        return;
    case 0x23:
        register_actor_segment_probe(actor, context.render.segment);
        register_actor_shape_probe(actor, context.render.shape);
        register_actor_model_anchors(actor, context.model_anchors);
        if (actor.link->head == nullptr)
        {
            append_actor_bind_child(context.world_bind, *actor.link);
        }
        if (actor.model != nullptr)
        {
            register_actor_render_bins(context.render);
        }
        restore_actor_bind_mode(actor, context.events.effects.default_position, context.events.effects.default_euler);
        dispatch_actor_resource_callout(state.resource, 0x47, nullptr, context.callouts);
        return;
    case 0x7C:
    {
        void* value = static_cast<const ActorReferenceMessage*>(message)->value;
        if (context.watched_actor == value)
        {
            context.watched_actor = nullptr;
        }
        if (value == &actor)
        {
            detach_actor_bind(actor);
            return;
        }
        break;
    }
    case 0x80:
        register_actor_child_list(context.render);
        clear_actor_live_attachments(context.live_attachments);
        return;
    case 0x88:
        update_actor_animation_pose(actor, state.animation, context.pose, state.bone,
            models.registry, context.skeleton, context.node_cache, context.reset);
        return;
    case 0x8B:
        update_actor_sound_pose(actor, *static_cast<RuntimeActorSound*>(static_cast<const ActorReferenceMessage*>(message)->value));
        break;
    default:
        break;
    }
}
}
