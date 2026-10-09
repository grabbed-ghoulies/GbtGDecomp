#pragma once

#include "../entities/aid_spawn_resources.h"

namespace gbtg
{
struct LiveSpawnTickContext
{
    AidSpawnQueueContext& queue;
    float& pause; // DAT_004312DC
    float& frame_delta; // DAT_005096C0
    float& time_scale; // DAT_004312C4
    u32& player_address; // DAT_00511B40, actor+0x93C
    const std::unordered_map<u32, Actor*>& actors;
};
void tick_live_spawn_queues(LiveSpawnTickContext& context); // Xbox 0x0011B670
}
