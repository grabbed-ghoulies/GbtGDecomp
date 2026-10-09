#include "actor_bind_tree.h"
#include "actor_transform.h"
#include "entity_heap.h"

#include <cmath>

namespace gbtg
{
// Xbox 0x0002CCD0
void append_actor_bind_child(ActorLink& parent, ActorLink& child)
{
    ActorLink* tail = parent.child;
    if (tail != nullptr)
    {
        auto* next = tail->next;
        while (next != nullptr)
        {
            tail = tail->next;
            next = tail->next;
        }
        tail->next = &child;
        child.head = tail->head;
        child.owner = tail->owner;
        return;
    }
    parent.child = &child;
    child.head = &parent;
    child.owner = parent.actor;
}

// Xbox 0x0002CD00
void unlink_actor_bind_node(ActorLink& node)
{
    auto* parent = static_cast<ActorLink*>(node.head);
    if (parent != nullptr)
    {
        auto* cursor = parent->child;
        ActorLink* preceding;
        do
        {
            preceding = cursor;
            if (preceding == nullptr)
            {
                parent->child = node.next;
                break;
            }
            cursor = preceding->next;
        }
        while (preceding->next != &node);
        if (preceding != nullptr)
        {
            preceding->next = node.next;
        }
        auto* nested = node.child;
        if (nested != nullptr)
        {
            ActorLink* last;
            do
            {
                last = nested;
                last->head = parent;
                last->owner = parent->actor;
                nested = last->next;
            }
            while (last->next != nullptr);
            if (preceding == nullptr)
            {
                parent->child = node.child;
            }
            else
            {
                preceding->next = node.child;
            }
            last->next = node.next;
        }
        node.head = nullptr;
        node.owner = nullptr;
        node.next = nullptr;
        node.child = nullptr;
    }
}

// Xbox 0x0003B8B0
Actor* actor_bound_parent(Actor& actor)
{
    ActorBind* bind = actor.bind;
    i32 mode;
    if (bind != nullptr)
    {
        if (bind->parent != nullptr)
        {
            mode = 1;
        }
        else if (actor.link != nullptr && actor.link->head != nullptr &&
            static_cast<ActorLink*>(actor.link->head)->head != nullptr)
        {
            mode = 2;
        }
        else
        {
            engine_heap_free(bind);
            actor.bind = nullptr;
            mode = 0;
        }
    }
    else
    {
        mode = 0;
    }
    if (mode == 1)
    {
        return bind->parent;
    }
    if (mode != 2)
    {
        return nullptr;
    }
    return actor.link->owner;
}

// Xbox 0x0003BB20
void bind_actor_under_parent(Actor& actor, Actor& parent, const Vec3& origin,
    const Vec3& euler)
{
    ActorLink* nested = actor.link->child;
    actor.link->child = nullptr;
    unlink_actor_bind_node(*actor.link);
    ActorLink* tail = parent.link->child;
    if (tail == nullptr)
    {
        parent.link->child = actor.link;
        actor.link->head = parent.link;
        actor.link->owner = &parent;
    }
    else
    {
        auto* next = tail->next;
        while (next != nullptr)
        {
            tail = tail->next;
            next = tail->next;
        }
        tail->next = actor.link;
        actor.link->head = tail->head;
        actor.link->owner = tail->owner;
    }
    if (nested != nullptr)
    {
        tail = actor.link->child;
        if (tail == nullptr)
        {
            actor.link->child = nested;
            nested->head = actor.link;
            nested->owner = &actor;
        }
        else
        {
            auto* next = tail->next;
            while (next != nullptr)
            {
                tail = tail->next;
                next = tail->next;
            }
            tail->next = nested;
            nested->head = tail->head;
            nested->owner = tail->owner;
        }
    }
    actor.position = origin;
    actor.xform_valid = 0;
    normalize_euler_angles(actor.euler, euler);
    actor.xform_valid = 0;
    actor_refresh_transform(actor);
}

// Xbox 0x0003B420
void restore_actor_bind_mode(Actor& actor, const Vec3& origin, const Vec3& euler)
{
    ActorBind* bind = actor.bind;
    if (bind != nullptr && bind->parent != nullptr && bind != nullptr)
    {
        bind->mode_local = bind->mode_parent;
        bind_actor_under_parent(actor, *bind->parent, origin, euler);
        bind->parent = nullptr;
    }
}

// Xbox 0x0003B560
void detach_actor_bind(Actor& actor)
{
    ActorBind* bind = actor.bind;
    i32 mode;
    if (bind != nullptr)
    {
        if (bind->parent != nullptr)
        {
            mode = 1;
        }
        else if (actor.link != nullptr && actor.link->head != nullptr &&
            static_cast<ActorLink*>(actor.link->head)->head != nullptr)
        {
            mode = 2;
        }
        else
        {
            engine_heap_free(bind);
            actor.bind = nullptr;
            mode = 0;
        }
    }
    else
    {
        mode = 0;
    }
    Actor* parent = actor_bound_parent(actor);
    if (mode == 1)
    {
        actor_refresh_transform(actor);
    }
    else if (mode != 2)
    {
        return;
    }
    const Vec3 position = actor_world_position(actor);
    float scale = actor.radius_xz;
    const float squared = bind->basis[8] * bind->basis[8] + bind->basis[4] * bind->basis[4] +
        bind->basis[0] * bind->basis[0];
    const float length = squared == 0.f ? 0.f : float(std::sqrt(static_cast<long double>(squared)));
    const float parent_scale = parent->scale_200;
    if (mode == 2)
    {
        ActorLink* nested = actor.link->child;
        actor.link->child = nullptr;
        unlink_actor_bind_node(*actor.link);
        auto* tail = parent->link;
        auto* next = parent->link->next;
        while (next != nullptr)
        {
            tail = tail->next;
            next = tail->next;
        }
        tail->next = actor.link;
        actor.link->head = tail->head;
        actor.link->owner = tail->owner;
        if (nested != nullptr)
        {
            append_actor_bind_child(*actor.link, *nested);
        }
    }
    const Vec3 euler = actor_world_euler(actor);
    engine_heap_free(bind);
    actor.bind = nullptr;
    normalize_euler_angles(actor.euler, euler);
    actor.xform_valid = 0;
    actor_set_world_position(actor, position);
    scale = (length / parent_scale) * scale;
    if (actor.scale_x != scale || actor.radius_y != scale || actor.radius_xz != scale)
    {
        actor.scale_x = scale;
        actor.radius_y = scale;
        actor.radius_xz = scale;
        actor.xform_valid = 0;
    }
    actor_refresh_transform(actor);
}
}
