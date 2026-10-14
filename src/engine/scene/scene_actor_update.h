#pragma once

#include "gbtg/actor_runtime.h"
#include "gbtg/entity.h"
#include "../entities/entity_types.h"

namespace gbtg
{
struct RuntimeSceneActorUpdates
{
    RuntimeActorLink*& current; // DAT_00511B60: next word of the +0x910 list head.
    const std::function<Actor&(RuntimeActorLink&)>& actor; // Actor owning the +0x910 node.
    const std::function<void(Actor&)>& update; // Actual vtable +0x10.
};
void update_runtime_scene_actors(RuntimeSceneActorUpdates& context); // Xbox 0x00123270

// The recurring actor updates of the scene tick 0x0011E7F0. An actor whose +0x908 node is linked
// (nonzero first word) is skipped; the others run vtable +0x10, then 0x00040DE0 and 0x0003D730.
struct RuntimeSceneActorTreeUpdates
{
    const std::function<bool(Actor&)>& retired; // Actor +0x908 word is nonzero.
    const std::function<void(Actor&)>& update; // Actual vtable +0x10.
};
// The bind tree under DAT_00511B10: each +0x918 node's +4 sibling, then its +8 child subtree.
void update_runtime_scene_actor_tree(ActorLink* node, RuntimeSceneActorTreeUpdates& context); // Xbox 0x00123020

struct RuntimeSceneBodyListUpdates
{
    entity_detail::ListHead& bodies; // DAT_00511B1C; DAT_00511B20 is the first +0x934 node.
    const std::function<Actor&(entity_detail::ListHead&)>& actor; // Actor owning the +0x934 node.
    RuntimeSceneActorTreeUpdates& updates;
};
void update_runtime_scene_body_list(RuntimeSceneBodyListUpdates& context); // Xbox 0x00122FE0
}
