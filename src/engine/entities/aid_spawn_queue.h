#pragma once

#include "spawn_queue_gates.h"

namespace gbtg
{
struct SpawnControllerSelection : SpawnPoolSelection
{
    u32 distinct;
    std::array<u8, 256> occurrences;
};
struct AidSpawnQueueRecord
{
    AidSpawnQueueRecord* previous;
    AidSpawnQueueRecord* next;
    SpawnPoolSelection* names; // +8
    SpawnPoolSelection* attributes; // +0xC
    SpawnControllerSelection* controllers; // +0x10
    SpawnPoolSelection* intervals; // +0x14, previous stores timer bits
    i32 remaining; // +0x18
    i16 value_1c;
    const char* current; // +0x20
    std::array<u8, 256> attempts; // +0x24
};
struct AidSpawnQueueContext
{
    GhoulySpawnContext& spawn;
    AnimationResourceRegistry& registry;
    ResourceErrorState& error;
    const std::unordered_map<const GhoulyController*, Actor*>& actors;
    i32& game_state; // DAT_005064D8
    i32& game_gate; // DAT_0050653C
};
bool tick_aid_spawn_queue(AidSpawnQueueRecord& record, const Vec3& position, float delta,
    AidSpawnQueueContext& context); // Xbox 0x000F4F80
}
