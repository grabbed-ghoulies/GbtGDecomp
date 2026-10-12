#include "scene_actor_activation.h"
#include "gbtg/entity.h"

namespace gbtg
{
// Xbox 0x00123070
void activate_runtime_scene_actors(RuntimeSceneActivation& context)
{
    if (context.pending.next == nullptr)
    {
        return;
    }
    const u32 activate[3]{0x23, 0x7B, 0};
    u32 notice[2]{0x7B, 0};
    bool first = true;
    do
    {
        RuntimeActorLink& node = *context.pending.next;
        const u32 actor = context.actor_address(node);
        // The first pass sends the words as they stand; later passes see the previous actor at +8.
        u32 words[3]{activate[0], activate[1], first ? activate[2] : notice[1]};
        first = false;
        context.message(actor, words);
        if (node.previous != nullptr)
        {
            node.previous->next = node.next;
        }
        if (node.next != nullptr)
        {
            node.next->previous = node.previous;
        }
        node.previous = nullptr;
        node.next = nullptr;
        auto& latch = context.active_node(node);
        latch.next = context.active;
        context.active = &latch;
        context.scene_nodes.push_back(actor);
        notice[1] = actor;
        Message message;
        message.id = 0x7B;
        message.arg = const_cast<Actor*>(context.actor_at(actor));
        for (auto* recipient = context.active; recipient != nullptr;)
        {
            auto* current = recipient;
            recipient = recipient->next;
            current->handle_message(*current, message);
        }
        for (auto* recipient = context.scene_controls; recipient != nullptr;)
        {
            auto* current = recipient;
            recipient = recipient->next;
            current->handle_message(*current, message);
        }
    } while (context.pending.next != nullptr);
}
}
