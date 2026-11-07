#pragma once
#include <array>
#include "strategy_target_queue.h"
#include "strategy_target_selection.h"
#include "strategy_fields.h"

namespace gbtg
{
// DAT_004288B0, read through the strategy's +0x38. Nothing in the dumps writes it.
struct ZombieParameters
{
    StrategyMoveSettings move; // +0x0
    float aimless_swipe_chance; // +0x18
    float unk_1C; // +0x1C
    float unk_20; // +0x20
    float sidestep_reach; // +0x24, a target this close sends it off to one side.
    float unk_28; // +0x28
    float unk_2C; // +0x2C
    float unk_30; // +0x30
    float unk_34; // +0x34
    float unk_38; // +0x38
    float unk_3C; // +0x3C
};
static_assert(sizeof(ZombieParameters) == 0x40);

// entityStrategyZombie (factory type 0x58, getter 0x000C57A0, vtable 0x0050609C) over the base strategy (type 8).
// It keeps the director's sizes (0x000AAE40, 0x44 bytes) and the empty destructor 0x001628B0.
// Strategy bytes: +0x1A attacking, +0x1C pause count, +0x20 target queue link, +0x38 the parameter block
// (DAT_004288B0), +0x3C the approach mode, +0x40 the special attack's hit.
struct ZombieStrategyState
{
    StrategyBase base{}; // +0
    u32 parameters = 0; // +0x38, DAT_004288B0, set by 0x000C57F0
    u32 approach = 0; // +0x3C, 0x000C51E0's mode: 1 walks at the target, 2 swipes aimlessly
    u32 special_hit = 0; // +0x40, message 0x3B in state 0x3EF
};
static_assert(sizeof(ZombieStrategyState) == 0x44);
struct RuntimeZombieStrategy
{
    RuntimeBaseStrategyMessages& base; // Base goals, entry and strategy link (+0x20).
    RuntimeStrategyTargetChecks& checks;
    RuntimeStrategyTargetQueue& queue;
    RuntimeStrategyActorViews& views;
    std::span<const u8> parameters; // DAT_004288B0, the block strategy +0x38 names.
    const char* goal_swipe_aimless; // "swipeaimless", 0x00321248.
    const char* goal_swipe; // "swipe", 0x00321258.
    const char* goal_special; // "special_attack", 0x00321260.
    const char* goal_kick; // "kickbig", 0x0031C6F4.
    const char* goal_get_weapon; // "getweapon", 0x00321420.
};
bool initialize_runtime_zombie_strategy(std::span<u8> strategy); // Xbox 0x000C57F0
bool runtime_zombie_body_state_shown(RuntimeZombieStrategy& context); // Xbox 0x000C53C0, EAX strategy
bool runtime_zombie_body_state_hidden(RuntimeZombieStrategy& context); // Xbox 0x000C5390, EAX strategy
void wait_runtime_zombie_weapon(RuntimeZombieStrategy& context); // Xbox 0x000C4E70, ESI strategy, state 0x405
void hold_runtime_zombie_target(RuntimeZombieStrategy& context); // Xbox 0x000C4EE0, ESI strategy, states 0x3ED/0x3EE
void approach_runtime_zombie_target(RuntimeZombieStrategy& context); // Xbox 0x000C4F40, ECX strategy, state 0x40C
RuntimePlayerMindRequest* request_runtime_zombie_flank(const RuntimeMindTarget& target, u32& beside,
    RuntimeZombieStrategy& context); // Xbox 0x000C5010, EAX actor
RuntimePlayerMindRequest* request_runtime_zombie_roam(RuntimeZombieStrategy& context); // Xbox 0x000C5190, EAX strategy
void set_runtime_zombie_approach(u32 mode, RuntimeZombieStrategy& context); // Xbox 0x000C51E0, EAX mode, ESI strategy
void begin_runtime_zombie_special(RuntimeZombieStrategy& context); // Xbox 0x000C5290, ESI strategy
void end_runtime_zombie_special(RuntimeZombieStrategy& context); // Xbox 0x000C52C0, EAX strategy
void update_runtime_zombie_special(RuntimeZombieStrategy& context); // Xbox 0x000C5300, ECX strategy, state 0x3EF
void update_runtime_zombie_wait(RuntimeZombieStrategy& context); // Xbox 0x000C5350, EDX strategy, state 0x416
void react_runtime_zombie_message(std::span<const u32> message,
    RuntimeZombieStrategy& context); // Xbox 0x000C5410, EAX strategy, EDI message
void return_runtime_zombie_to_approach(RuntimeZombieStrategy& context); // Xbox 0x000C5570, ESI strategy
u16 choose_runtime_zombie_attack(RuntimeZombieStrategy& context); // Xbox 0x000C55D0, EAX strategy
void choose_runtime_zombie_state(RuntimeZombieStrategy& context); // Xbox 0x000C5670, EAX strategy
u16 choose_runtime_zombie_weapon_state(RuntimeZombieStrategy& context); // Xbox 0x000C5730, ESI strategy
void update_runtime_zombie_strategy(RuntimeZombieStrategy& context); // Xbox 0x000C5810, vtable +0x10
void dispatch_runtime_zombie_message(std::span<const u32> message,
    RuntimeZombieStrategy& context); // Xbox 0x000C5920, vtable +0xC
void enter_runtime_zombie_state(u16 state, u16 previous,
    RuntimeZombieStrategy& context); // Xbox 0x000C5970, vtable +0x14
void leave_runtime_zombie_state(u16 state, u16 next, RuntimeZombieStrategy& context); // Xbox 0x000C5AD0, vtable +0x18
}
