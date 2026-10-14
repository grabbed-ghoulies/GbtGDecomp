#pragma once

#include "gbtg/actor_runtime.h"

namespace gbtg
{
struct RuntimeSceneControlUpdates
{
    RuntimeActorLink*& pending; // DAT_0050E440, object +8 node.
    RuntimeActorLink& active; // DAT_0050E434, object +8 head.
    RuntimeActorLink& updates; // DAT_0050E444, object +0x10 head.
    const std::function<RuntimeActorLink&(RuntimeActorLink&)>& update_link;
    const std::function<void(RuntimeActorLink&, std::span<const u32>)>& message;
    const std::function<void(RuntimeActorLink&)>& update;
};
void activate_runtime_scene_controls(RuntimeSceneControlUpdates& context); // Xbox 0x00115FF0
void notify_runtime_scene_controls(RuntimeSceneControlUpdates& context); // Xbox 0x00116090
void update_runtime_scene_controls(RuntimeSceneControlUpdates& context); // Xbox 0x001160F0
// Scene start empties the DAT_0050E444 update list without running the updates.
void clear_runtime_scene_control_updates(RuntimeSceneControlUpdates& context); // Xbox 0x001160C0
}
