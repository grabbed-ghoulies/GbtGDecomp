#pragma once
#include <array>
#include "strategy_target_queue.h"
#include "strategy_target_selection.h"
#include "strategy_fields.h"

namespace gbtg
{
// entityStrategySkeleton (factory type 0x51, getter 0x000BFA70, vtable 0x00505FD8) over the base strategy (type 8).
// Its constructor is the shared 0x000CFFD0 and its destructor the empty 0x001628B0.
// Strategy bytes: +0x1A attacking, +0x1C pause count, +0x20 target queue link, +0x38 no spawn in reach,
// +0x3C the delay before the next weapon fetch (state 0x405).
// The skeleton's own strategy fields at their retail offsets, after the base strategy's.
struct SkeletonStrategyState
{
    StrategyBase base{}; // +0
    u32 no_spawn_in_reach = 0; // +0x38, set by 0x000C0430 when no weapon spawn is in reach
    float fetch_delay = 0.f; // +0x3C, the delay before the next weapon fetch (state 0x405)
};
static_assert(sizeof(SkeletonStrategyState) == 0x40);
struct RuntimeSkeletonStrategy
{
    RuntimeBaseStrategyMessages& base; // Base goals, entry and strategy link (+0x20).
    RuntimeStrategyTargetChecks& checks;
    RuntimeStrategyTargetQueue& queue;
    RuntimeStrategyActorViews& views;
    const float& fetch_delay; // DAT_004288AC.
    const char* goal_attack; // "attack", 0x003211E4.
    const char* goal_knockdown; // "knockdown", 0x003217C0.
    const char* goal_projectile; // "attackwithprojectile", 0x0032138C.
    const char* goal_weapon; // "attackwithweapon", 0x00321378.
    const char* goal_dodge; // "duckndodge?", 0x00324310.
    const char* goal_far; // "far", 0x00321824.
    const char* goal_get_weapon; // "getweapon", 0x00321420.
};
void skeleton_strategy_object_sizes(ObjectSizes& output); // Xbox 0x000A9FC0
bool runtime_skeleton_delay_runs(RuntimeSkeletonStrategy& context); // Xbox 0x000C05C0, EAX actor
bool join_runtime_skeleton_target(RuntimeMindTarget& target,
    RuntimeSkeletonStrategy& context); // Xbox 0x000C0170, EAX strategy, EDI target
bool select_runtime_skeleton_flank(Vec3* output, RuntimeSkeletonStrategy& context); // Xbox 0x000C01A0, EAX strategy
void reposition_runtime_skeleton(RuntimeSkeletonStrategy& context); // Xbox 0x000C02D0, EDI strategy
bool runtime_skeleton_target_crowded(const RuntimeMindTarget* target,
    RuntimeSkeletonStrategy& context); // Xbox 0x000C03D0, EAX target, ECX strategy
u16 choose_runtime_skeleton_attack(const RuntimeMindTarget& target,
    RuntimeSkeletonStrategy& context); // Xbox 0x000C0430, ESI strategy
void engage_runtime_skeleton(RuntimeSkeletonStrategy& context); // Xbox 0x000C0500, ESI strategy
void set_runtime_skeleton_stance(u8 stance,
    RuntimeSkeletonStrategy& context); // Xbox 0x000C0590, EAX strategy, ECX body
void dispatch_runtime_skeleton_message(std::span<const u32> message,
    RuntimeSkeletonStrategy& context); // Xbox 0x000BFE10, vtable +0xC
void update_runtime_skeleton_strategy(RuntimeSkeletonStrategy& context); // Xbox 0x000BFAC0, vtable +0x10
void enter_runtime_skeleton_state(u16 state, u16 previous,
    RuntimeSkeletonStrategy& context); // Xbox 0x000BFF80, vtable +0x14
void leave_runtime_skeleton_state(u16 state, RuntimeSkeletonStrategy& context); // Xbox 0x000C0130, vtable +0x18
}
