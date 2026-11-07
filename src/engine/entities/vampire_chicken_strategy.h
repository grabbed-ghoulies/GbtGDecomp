#pragma once
#include <array>
#include "strategy_fields.h"
#include "spider_strategy.h"

namespace gbtg
{
// entityStrategyVampireChicken (factory type 0x55, getter 0x000C28E0, vtable 0x00506048; the factory table's row 0x54
// carries the name) over the base strategy (type 8): a chicken turned vampire. It emerges (state 0x3FD, "emerge"),
// wanders ("randomwander") and, queued on a standing target, either grabs it ("knockbackprepare", state 0x3ED, holding
// the target's body +0x50) or knocks it down ("knockdown", 0x3EE). It keeps the director's sizes (0x000AAE40,
// 0x44 / 0x18) and the empty destructor 0x001628B0.
struct VampireChickenStrategyState
{
    StrategyBase base{}; // +0
    u32 parameters = 0; // +0x38, DAT_003034E8
    u32 held = 0; // +0x3C, the grabbed target's body (its +0x50 names this actor)
    u32 provoked = 0; // +0x40, each tick: the actor is provoked, or its target is no player in view
};
static_assert(sizeof(VampireChickenStrategyState) == 0x44);
struct RuntimeVampireChickenStrategy
{
    // The instance's spider views over the same strategy: base goals, target checks, queue and actor views.
    RuntimeSpiderStrategy& shared;
    const char* goal_emerge; // "emerge", 0x0031C5C4.
    const char* goal_wander; // "randomwander", 0x003217B0.
    const char* goal_grab; // "knockbackprepare", 0x003212AC.
    const char* goal_knockdown; // "knockdown", 0x003217C0.
    const Actor* held_actor = nullptr; // The actor whose body +0x3C names (the body's +8).
};
bool initialize_runtime_vampire_chicken_strategy(std::span<u8> strategy); // Xbox 0x000C2930
void update_runtime_vampire_chicken_strategy(RuntimeVampireChickenStrategy& context); // Xbox 0x000C2950, vtable +0x10
void dispatch_runtime_vampire_chicken_message(std::span<const u32> message,
    RuntimeVampireChickenStrategy& context); // Xbox 0x000C2A40, vtable +0xC
void enter_runtime_vampire_chicken_state(u16 state, u16 previous,
    RuntimeVampireChickenStrategy& context); // Xbox 0x000C2C70, vtable +0x14
void leave_runtime_vampire_chicken_state(u16 state,
    RuntimeVampireChickenStrategy& context); // Xbox 0x000C2D10, vtable +0x18
}
