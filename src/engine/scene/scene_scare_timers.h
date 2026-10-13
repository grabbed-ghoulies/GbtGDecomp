#pragma once
#include "../entities/powerup_awards.h"
#include "../entities/actor_clock.h"

namespace gbtg
{
struct RuntimeSceneScareTimers
{
    RuntimePowerupTable& powerups;
    i32& count; // DAT_00512320.
    std::span<u32, 4> actors; // DAT_00512324.
    std::span<float, 4> times; // DAT_00512334.
    const RuntimeActorClock& clock;
    const std::function<void(u32, std::span<const u32>)>& message; // Vtable +0xC.
};
void remove_runtime_scared_actor(u32 actor, RuntimeSceneScareTimers& context); // Xbox 0x00123E00
void tick_runtime_scene_scare_timers(RuntimeSceneScareTimers& context); // Xbox 0x00123BA0
}
