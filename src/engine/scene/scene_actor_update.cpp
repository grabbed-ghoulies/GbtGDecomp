#include "scene_actor_update.h"
#include "gbtg/entity.h"
#include "../entities/actor_pose_updates.h"

namespace gbtg
{
// Xbox 0x00123270
void update_runtime_scene_actors(RuntimeSceneActorUpdates& context)
{
    auto* node = context.current;
    while (node != nullptr)
    {
        Actor& actor = context.actor(*node);
        context.current = node;
        if (node->previous != nullptr)
        {
            node->previous->next = node->next;
        }
        if (node->next != nullptr)
        {
            node->next->previous = node->previous;
        }
        node->previous = nullptr;
        node->next = nullptr;
        context.update(actor);
        actor_refresh_transform(actor);
        node = context.current;
    }
    context.current = node;
}

// Xbox 0x00123020
void update_runtime_scene_actor_tree(ActorLink* node, RuntimeSceneActorTreeUpdates& context)
{
    for (; node != nullptr; node = node->next)
    {
        Actor& actor = *node->actor;
        if (!context.retired(actor))
        {
            context.update(actor);
            actor_refresh_transform(actor);
            update_actor_model_factor(actor);
        }
        if (node->child != nullptr)
        {
            update_runtime_scene_actor_tree(node->child, context);
        }
    }
}

// Xbox 0x00122FE0
void update_runtime_scene_body_list(RuntimeSceneBodyListUpdates& context)
{
    auto* node = static_cast<entity_detail::ListHead*>(context.bodies.next);
    while (node != nullptr)
    {
        Actor& actor = context.actor(*node);
        node = static_cast<entity_detail::ListHead*>(node->next);
        if (!context.updates.retired(actor))
        {
            context.updates.update(actor);
            actor_refresh_transform(actor);
            update_actor_model_factor(actor);
        }
    }
}
}
