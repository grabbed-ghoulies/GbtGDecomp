#pragma once
#include "../frontend/scoreboard_messages.h"
#include "ghouly_spawn_record.h"

namespace gbtg
{
// One row of the powerup table (DAT_00512140, stride 0x18, indexed by powerup kind 0..0x13).
struct RuntimePowerupRow
{
    u32 active; // +0.
    float amount; // +4.
    u32 extra; // +8.
    u32 player; // +0xC, actor address.
    i32 id; // +0x10.
    u32 message; // +0x14, scoreboard message id.
};
struct RuntimePowerupTable
{
    std::span<RuntimePowerupRow, 20> rows; // DAT_00512140.
    RuntimeScoreboard*& board; // DAT_00545ABC.
    AnimationResourceRegistry& registry;
};
void cancel_runtime_powerup(u32 kind, RuntimePowerupTable& table); // Xbox 0x00124320, EAX kind
// Adds time to the kind 0x10 and 0x11 entries (stride 0x14, time at +8) of the DAT_004E17D8 pool.
void extend_runtime_scene_timers(float amount, GhoulySpawnPool* timers); // Xbox 0x0004A370, XMM0 amount
// The challenge record (stride 0x88) whose kind word at +0 matches, from the DAT_005064E0 pool.
u8* find_runtime_challenge_record(u32 kind, GhoulySpawnPool* challenges); // Xbox 0x000D0650, EDX kind
}
