#pragma once

#include "ghouly_spawn_scheduler.h"
#include "spawn_pool_selection.h"
#include "gbtg/resource_error.h"

namespace gbtg
{
bool spawn_queue_below_minimum(const GhoulySpawnContext& context); // Xbox 0x0013E7E0
bool spawn_queue_below_maximum(const GhoulySpawnContext& context); // Xbox 0x0013E790
bool spawn_script_under_class_limits(const u8* records,
    const GhoulySpawnContext& context); // Xbox 0x00051900
bool spawn_box_under_class_limits(const char* name, AnimationResourceRegistry& registry,
    ResourceErrorState& error, const GhoulySpawnContext& context); // Xbox 0x0013EB10
bool tick_spawn_queue_interval(SpawnPoolSelection& selection, float& timer, float delta,
    const char* name, AnimationResourceRegistry& registry, ResourceErrorState& error,
    GhoulySpawnContext& context); // Xbox 0x000F5200
}
