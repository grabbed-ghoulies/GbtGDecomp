#pragma once

#include "gbtg/entity.h"

namespace gbtg
{
void append_actor_bind_child(ActorLink& parent, ActorLink& child); // Xbox 0x0002CCD0
void unlink_actor_bind_node(ActorLink& node); // Xbox 0x0002CD00
Actor* actor_bound_parent(Actor& actor); // Xbox 0x0003B8B0
void bind_actor_under_parent(Actor& actor, Actor& parent, const Vec3& origin,
    const Vec3& euler); // Xbox 0x0003BB20
void restore_actor_bind_mode(Actor& actor, const Vec3& origin, const Vec3& euler); // Xbox 0x0003B420
void detach_actor_bind(Actor& actor); // Xbox 0x0003B560
}
