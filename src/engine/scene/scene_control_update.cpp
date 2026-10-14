#include "scene_control_update.h"

namespace gbtg
{
// Xbox 0x00115FF0
void activate_runtime_scene_controls(RuntimeSceneControlUpdates& context)
{
    auto* node = context.pending;
    while (node != nullptr)
    {
        context.pending = node;
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
        auto* active = &context.active;
        while (active->previous != nullptr)
        {
            active = active->previous;
        }
        auto* next = active->next;
        active->next = node;
        node->previous = active;
        node->next = next;
        if (next != nullptr)
        {
            next->previous = node;
        }
        auto* tail = &context.updates;
        if (context.updates.next != nullptr)
        {
            tail = context.updates.next;
            while (tail->next != nullptr)
            {
                tail = tail->next;
            }
        }
        auto& update = context.update_link(*node);
        tail->next = &update;
        update.previous = tail;
        update.next = nullptr;
        node = context.pending;
    }
    context.pending = node;
}

// Xbox 0x00116090
void notify_runtime_scene_controls(RuntimeSceneControlUpdates& context)
{
    if (context.updates.next != nullptr)
    {
        const u32 message[1]{0x7D};
        auto* node = context.updates.next;
        do
        {
            context.message(*node, message);
            node = node->next;
        }
        while (node != nullptr);
    }
}

// Xbox 0x001160F0
void update_runtime_scene_controls(RuntimeSceneControlUpdates& context)
{
    auto* node = context.updates.next;
    while (node != nullptr)
    {
        context.updates.next = node;
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
        context.update(*node);
        node = context.updates.next;
    }
    context.updates.next = node;
}

// Xbox 0x001160C0
void clear_runtime_scene_control_updates(RuntimeSceneControlUpdates& context)
{
    while (context.updates.next != nullptr)
    {
        RuntimeActorLink* node = context.updates.next;
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
    }
}
}
