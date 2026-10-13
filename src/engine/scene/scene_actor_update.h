#pragma once

#include "gbtg/actor_runtime.h"

namespace gbtg
{
struct RuntimeSceneActorUpdates
{
    RuntimeActorLink*& current; // DAT_00511B60: next word of the +0x910 list head.
    const std::function<Actor&(RuntimeActorLink&)>& actor; // Actor owning the +0x910 node.
    const std::function<void(Actor&)>& update; // Actual vtable +0x10.
};
void update_runtime_scene_actors(RuntimeSceneActorUpdates& context); // Xbox 0x00123270
}
