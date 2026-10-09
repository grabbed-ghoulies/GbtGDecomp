#pragma once

#include "ghouly_spawn_record.h"
#include "ghouly_requests.h"
#include "gbtg/entity.h"

#include <unordered_map>

namespace gbtg
{
struct SpawnPoolSelection
{
    GhoulySpawnPool* pool;
    u32 mode;
    u32 sequence;
    i32 previous;
};

u8* select_spawn_name(SpawnPoolSelection& selection, u32& random_state); // Xbox 0x000F3840
u8* select_spawn_attributes(SpawnPoolSelection& selection, u32& random_state); // Xbox 0x000F36C0
float select_spawn_interval(SpawnPoolSelection& selection, u32& random_state); // Xbox 0x000F5280
i32 select_unused_spawn_controller(SpawnPoolSelection& selection, u32& random_state); // Xbox 0x000F5610
i32 count_spawn_controller_attempts(std::span<const u8, 256> attempts); // Xbox 0x000F50D0
i32 select_nearest_spawn_controller(const SpawnPoolSelection& selection, const Vec3& position,
    const std::unordered_map<const GhoulyController*, Actor*>& actors); // Xbox 0x000F5530
u32 select_spawn_controller(SpawnPoolSelection& selection, const Vec3& position, u32& random_state,
    const std::unordered_map<const GhoulyController*, Actor*>& actors); // Xbox 0x000F5410
}
