#pragma once

#include "entity_types.h"
#include "gbtg/anim.h"

namespace gbtg
{
const u8* find_actor_model_binding(const Actor& actor, i16 kind); // Xbox 0x000408D0
struct ActorModelAnchor
{
    entity_detail::ListHead owner_link; // +0
    entity_detail::ListHead global_link; // +8
    Actor* owner; // +0x10
    i32 mode; // +0x14
    const u8* binding; // +0x18 when mode 0
    u16 joint; // native interpretation of +0x18 for modes 1/2
    Vec3 local; // +0x1C
    Vec3 output; // +0x28
    float radius; // +0x34
};
extern entity_detail::ListHead g_actor_model_anchors; // DAT_00510654
ActorModelAnchor* allocate_actor_model_anchor(Actor& actor, entity_detail::ListHead& owner_head,
    i32 mode); // Xbox 0x0011C0B0
void update_actor_model_anchor(ActorModelAnchor& anchor); // Xbox 0x0011BE40
void register_actor_model_anchors(Actor& actor, entity_detail::ListHead& owner_head); // Xbox 0x0003EEB0
struct ActorModelAnchorReleaseMessage
{
    i32 id;
    ActorModelAnchor* anchor;
};
void release_actor_model_anchor(ActorModelAnchor& anchor); // Xbox 0x0011BDA0
}
