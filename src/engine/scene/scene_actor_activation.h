#pragma once
#include "gbtg/actor_runtime.h"
#include "../entities/entity_types.h"
#include <functional>

namespace gbtg
{
// Scene startup's activation of the actors created since the last pass (0x00123070).
struct RuntimeSceneActivation
{
    RuntimeActorLink& pending; // DAT_00511B08, actor +0x900 nodes.
    entity_detail::LatchNode*& active; // DAT_00511AA4, actor +0x900 nodes once active.
    std::vector<u32>& scene_nodes; // DAT_00511B5C, actor +0x910 nodes in list order (retail actor addresses).
    entity_detail::LatchNode*& scene_controls; // DAT_0050E438, object +8 nodes.
    std::function<u32(RuntimeActorLink&)> actor_address; // Actor of a pending node.
    std::function<entity_detail::LatchNode&(RuntimeActorLink&)> active_node; // The same actor's +0x900 node.
    std::function<void(u32, std::span<const u32>)> message; // Actor vtable +0xC.
    std::function<const Actor*(u32)> actor_at; // Native actor for a Message argument.
};
void activate_runtime_scene_actors(RuntimeSceneActivation& context); // Xbox 0x00123070
}
