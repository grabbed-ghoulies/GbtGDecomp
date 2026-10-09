#pragma once

#include "aid_spawn_queue.h"

namespace gbtg
{
extern AidSpawnQueueRecord g_aid_spawn_queue; // DAT_00510570
SpawnPoolSelection* allocate_spawn_string_selection(); // Xbox 0x000F3640
void append_spawn_string(SpawnPoolSelection& selection, const char* name); // Xbox 0x000F3690
void release_spawn_selection(SpawnPoolSelection* selection); // Xbox 0x000F37B0
SpawnControllerSelection* allocate_spawn_controller_selection(); // Xbox 0x000F5370
void append_spawn_controller(SpawnControllerSelection& selection, u32 key); // Xbox 0x000F53D0
void set_spawn_interval_range(SpawnPoolSelection& selection, float low, float high); // Xbox 0x000F5110
void set_spawn_interval_ramp(SpawnPoolSelection& selection, float initial, float limit,
    float step); // Xbox 0x000F5170
void decode_aid_spawn_queue(AidSpawnQueueRecord& record, const u8* resource); // Xbox 0x00051940
AidSpawnQueueRecord* load_aid_spawn_queue(const char* name, AnimationResourceRegistry& registry,
    ResourceErrorState& error); // Xbox 0x000F4E50
void release_aid_spawn_queue(AidSpawnQueueRecord* record); // Xbox 0x000F4EE0
}
