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

// Xbox 0x001231F0
void send_runtime_scene_start_notice(RuntimeSceneActivation& context)
{
    if (!context.scene_nodes.empty())
    {
        const u32 notice[1]{0x88};
        for (size_t index = 0; index < context.scene_nodes.size(); ++index)
        {
            context.message(context.scene_nodes[index], notice);
        }
    }
}

// Xbox 0x001231A0
void send_runtime_scene_start(RuntimeSceneActivation& context)
{
    if (!context.scene_nodes.empty())
    {
        const u32 start[1]{0x7D};
        for (size_t index = 0; index < context.scene_nodes.size(); ++index)
        {
            context.message(context.scene_nodes[index], start);
        }
    }
}

// Xbox 0x00123240
void clear_runtime_scene_start_list(RuntimeSceneActivation& context)
{
    // DAT_00511B5C is held as the ordered node addresses; unlinking the first until none remain empties it.
    while (!context.scene_nodes.empty())
    {
        context.scene_nodes.erase(context.scene_nodes.begin());
    }
}
}
