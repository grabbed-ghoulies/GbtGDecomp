#include "actor_render_registration.h"
#include "entity.h"
#include <cstring>

namespace gbtg
{
// Xbox 0x0003E8C0
void register_actor_wide_list(ActorRenderRegistration& state)
{
    if (state.wide.prev == nullptr)
    {
        auto* root = &entity_detail::g_world.wide;
        while (root->prev != nullptr)
        {
            root = static_cast<entity_detail::ListHead*>(root->prev);
        }
        auto* successor = static_cast<entity_detail::ListHead*>(root->next);
        root->next = &state.wide;
        state.wide.prev = root;
        state.wide.next = successor;
        if (successor != nullptr)
        {
            successor->prev = &state.wide;
        }
    }
}

// Xbox 0x0003E900, EAX actor
void unregister_actor_wide_list(ActorRenderRegistration& state)
{
    if (state.wide.prev != nullptr)
    {
        static_cast<entity_detail::ListHead*>(state.wide.prev)->next = state.wide.next;
    }
    if (state.wide.next != nullptr)
    {
        static_cast<entity_detail::ListHead*>(state.wide.next)->prev = state.wide.prev;
    }
    state.wide.prev = nullptr;
    state.wide.next = nullptr;
}

// Xbox 0x0003EE70
void unlink_actor_view(RuntimeActorLink& link)
{
    if (link.previous != nullptr)
    {
        // The predecessor may be the native list sentinel with the same pointer slots.
        std::memcpy(reinterpret_cast<u8*>(link.previous) + sizeof(void*),
            &link.next, sizeof(link.next));
    }
    if (link.next != nullptr)
    {
        link.next->previous = link.previous;
    }
    link.previous = nullptr;
    link.next = nullptr;
}

// Xbox 0x0003EE30, EDX actor
void link_actor_view(RuntimeActorLink& live, RuntimeActorLink& link)
{
    RuntimeActorLink* tail = &live;
    while (tail->next != nullptr)
    {
        tail = tail->next;
    }
    tail->next = &link;
    link.previous = tail;
    link.next = nullptr;
}

// Xbox 0x00112D10
void activate_actor_bind_children(ActorLink* node, Actor& source)
{
    while (node != nullptr)
    {
        if (node->child != nullptr)
        {
            activate_actor_bind_children(node->child, source);
        }
        ActorRenderRegistration& state = *node->actor->render_registration;
        node = node->next;
        if (state.child.prev == nullptr)
        {
            state.message(state, 0x80);
            if (state.child.prev == nullptr)
            {
                register_actor_child_list(state);
            }
        }
    }
}

// Xbox 0x00112CC0
void activate_actor_bind_tree(Actor& actor)
{
    activate_actor_bind_children(actor.link->child, actor);
    ActorRenderRegistration& state = *actor.render_registration;
    if (state.child.prev == nullptr)
    {
        state.message(state, 0x80);
        if (state.child.prev == nullptr)
        {
            register_actor_child_list(state);
        }
    }
}

// Xbox 0x0003CAE0
void register_actor_render_bins(ActorRenderRegistration& state)
{
    bool changed = false;
    auto* node = state.bins;
    auto* bin = entity_detail::g_world.bins;
    do
    {
        i32 remaining = 4;
        do
        {
            if (node->prev == nullptr)
            {
                auto* root = bin;
                while (root->prev != nullptr)
                {
                    root = static_cast<entity_detail::ListHead*>(root->prev);
                }
                auto* successor = static_cast<entity_detail::ListHead*>(root->next);
                root->next = node;
                node->prev = root;
                node->next = successor;
                if (successor != nullptr)
                {
                    successor->prev = node;
                }
                changed = true;
            }
            ++node;
            ++bin;
            --remaining;
        }
        while (remaining != 0);
    }
    while (bin < entity_detail::g_world.bins + 8);
    if (state.actor.model != nullptr && state.actor.model->cpu.data() != nullptr &&
        read_u32(state.actor.model->cpu, read_u32(state.actor.model->cpu, 0x10) + 8) != 0)
    {
        auto* tail = &entity_detail::g_world.anchor;
        if (tail->next != nullptr)
        {
            do
            {
                tail = static_cast<entity_detail::ListHead*>(tail->next);
            }
            while (tail->next != nullptr);
        }
        tail->next = &state.anchor;
        state.anchor.prev = tail;
        state.anchor.next = nullptr;
    }
    if (changed)
    {
        state.message(state, 0x11);
    }
}

// Xbox 0x0003CBB0
void unregister_actor_render_bins(ActorRenderRegistration& state)
{
    bool changed = false;
    auto* node = state.bins;
    i32 groups = 2;
    do
    {
        if (node[0].prev != nullptr)
        {
            static_cast<entity_detail::ListHead*>(node[0].prev)->next = node[0].next;
            if (node[0].next != nullptr)
            {
                static_cast<entity_detail::ListHead*>(node[0].next)->prev = node[0].prev;
            }
            node[0].prev = nullptr;
            node[0].next = nullptr;
            changed = true;
        }
        if (node[1].prev != nullptr)
        {
            static_cast<entity_detail::ListHead*>(node[1].prev)->next = node[1].next;
            if (node[1].next != nullptr)
            {
                static_cast<entity_detail::ListHead*>(node[1].next)->prev = node[1].prev;
            }
            node[1].prev = nullptr;
            node[1].next = nullptr;
            changed = true;
        }
        if (node[2].prev != nullptr)
        {
            static_cast<entity_detail::ListHead*>(node[2].prev)->next = node[2].next;
            if (node[2].next != nullptr)
            {
                static_cast<entity_detail::ListHead*>(node[2].next)->prev = node[2].prev;
            }
            node[2].prev = nullptr;
            node[2].next = nullptr;
            changed = true;
        }
        if (node[3].prev != nullptr)
        {
            static_cast<entity_detail::ListHead*>(node[3].prev)->next = node[3].next;
            if (node[3].next != nullptr)
            {
                static_cast<entity_detail::ListHead*>(node[3].next)->prev = node[3].prev;
            }
            node[3].prev = nullptr;
            node[3].next = nullptr;
            changed = true;
        }
        node += 4;
        --groups;
    }
    while (groups != 0);
    if (state.anchor.prev != nullptr)
    {
        static_cast<entity_detail::ListHead*>(state.anchor.prev)->next = state.anchor.next;
    }
    if (state.anchor.next != nullptr)
    {
        static_cast<entity_detail::ListHead*>(state.anchor.next)->prev = state.anchor.prev;
    }
    state.anchor.prev = nullptr;
    state.anchor.next = nullptr;
    if (changed)
    {
        state.message(state, 0x12);
    }
}

// Xbox 0x0003E850
void unregister_actor_segment_probes(ActorRenderRegistration& state)
{
    state.actor.model_flags = 0;
    unlink_actor_segment_lists(state);
}

// Xbox 0x0003E7A0
void unlink_actor_segment_lists(ActorRenderRegistration& state)
{
    auto& world = entity_detail::g_world;
    auto* sentinel = reinterpret_cast<entity_detail::SegmentProbe*>(&world.segment_prev);
    if (state.segment.prev != nullptr)
    {
        auto** next = state.segment.prev == sentinel ? &world.segment : &state.segment.prev->next;
        *next = state.segment.next;
    }
    if (state.segment.next != nullptr)
    {
        state.segment.next->prev = state.segment.prev;
    }
    state.segment.prev = nullptr;
    state.segment.next = nullptr;
    if (state.wide.prev != nullptr)
    {
        static_cast<entity_detail::ListHead*>(state.wide.prev)->next = state.wide.next;
    }
    if (state.wide.next != nullptr)
    {
        static_cast<entity_detail::ListHead*>(state.wide.next)->prev = state.wide.prev;
    }
    state.wide.prev = nullptr;
    state.wide.next = nullptr;
}

// Xbox 0x0003ED70
void register_actor_child_list(ActorRenderRegistration& state)
{
    if (state.child.prev == nullptr)
    {
        auto& world = entity_detail::g_world;
        auto* tail = &world.child;
        if (world.child.next != nullptr)
        {
            do
            {
                tail = static_cast<entity_detail::ListHead*>(tail->next);
            }
            while (tail->next != nullptr);
        }
        tail->next = &state.child;
        state.child.prev = tail;
        state.child.next = nullptr;
        unregister_actor_render_bins(state);
        unregister_actor_segment_probes(state);
        clear_actor_shape_probe(state.actor, state.shape);
        auto* root = state.view.previous;
        if (root == nullptr)
        {
            root = nullptr;
        }
        else
        {
            do
            {
                auto* previous = root->previous;
                if (previous == nullptr)
                {
                    break;
                }
                root = previous;
            }
            while (true);
        }
        auto* sentinel = reinterpret_cast<RuntimeActorLink*>(&world.view);
        if (root == sentinel)
        {
            if (state.view.previous != nullptr)
            {
                if (state.view.previous == sentinel)
                {
                    world.view.next = state.view.next;
                }
                else
                {
                    state.view.previous->next = state.view.next;
                }
            }
            if (state.view.next != nullptr)
            {
                state.view.next->previous = state.view.previous;
            }
            state.view.previous = nullptr;
            state.view.next = nullptr;
        }
    }
}
}
