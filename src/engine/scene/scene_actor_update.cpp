#include "scene_actor_update.h"
#include "gbtg/entity.h"

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
}
