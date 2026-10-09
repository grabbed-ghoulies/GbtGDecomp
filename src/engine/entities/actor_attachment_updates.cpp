#include "actor_attachment_requests.h"
#include <cstring>

namespace gbtg
{
// Xbox 0x0003CD00
void update_actor_attachment_effects(Actor& actor, ActorAttachmentList& active,
    ActorAttachmentNode& pending, ActorEffectStreamContext& context)
{
    ActorAttachmentNode* node = pending.next;
    while (node != nullptr)
    {
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
        if (!start_actor_attachment_effect(actor, *node, context))
        {
            engine_heap_free(node);
        }
        else
        {
            ActorAttachmentNode* tail = &active.sentinel;
            while (tail->next != nullptr)
            {
                tail = tail->next;
            }
            tail->next = node;
            node->previous = tail;
            node->next = nullptr;
        }
        node = pending.next;
    }
    for (node = active.sentinel.next; node != nullptr; node = node->next)
    {
        i32 slot;
        std::memcpy(&slot, node->bytes.data() + 8, sizeof(slot));
        ActorEffectNode* effect = g_actor_effect_nodes[slot];
        Vec3 position;
        Vec3 euler;
        actor_attachment_pose(actor, *node, position, euler);
        if ((node->bytes[0x20] & 1) != 0)
        {
            std::memcpy(effect->bytes.data() + 0x1C, &position, sizeof(position));
        }
        if ((node->bytes[0x20] & 2) != 0)
        {
            std::memcpy(effect->bytes.data() + 0x50, &euler, sizeof(euler));
        }
    }
}
}
