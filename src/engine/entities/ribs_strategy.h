#pragma once
#include "body_action_selection.h"
#include "player_strategy_goals.h"
#include "strategy_target_selection.h"

#include <functional>

namespace gbtg
{
// entityStrategyRibs (factory type 0x50, getter 0x000BF760, vtable 0x00505FBC; the factory table's row 0x4F carries
// the name) over the base strategy (type 8): Ribs keeps its distance ("far", state 0x401) until a target is selected
// within 65, then holds it off (state 0x40C) with mind request 0xD, whose last float grows as its health falls. Its
// sizes are the base strategy's 0x0002B6A0, +8 the empty 0x001628B0 and +0x18 the base strategy's leave 0x000BFA50.
// Strategy bytes: +0x1A / +0x1B cleared on every entry, +0x1C pause count.
struct RuntimeRibsStrategy
{
    RuntimeBaseStrategyMessages& base; // Base goals, entry and strategy link (+0x20).
    RuntimeStrategyTargetChecks& checks; // 0x001384F0.
    RuntimeBodyActionSelection& actions; // 0x00027650.
    std::function<u16()> action_key; // Body vtable +0x20.
    std::function<void(u32, u8)> model_nodes; // 0x000F96F0 on the +0x204 and +0x310 models (stack slot, AL value).
    const char* goal_far; // "far", 0x00321824.
};
bool initialize_runtime_ribs_strategy(RuntimeRibsStrategy& context); // Xbox 0x000BF7B0
void update_runtime_ribs_strategy(RuntimeRibsStrategy& context); // Xbox 0x000BF7F0, vtable +0x10
void dispatch_runtime_ribs_message(std::span<const u32> message,
    RuntimeRibsStrategy& context); // Xbox 0x000BF8B0, vtable +0xC
void enter_runtime_ribs_state(u16 state, u16 previous, RuntimeRibsStrategy& context); // Xbox 0x000BF910, vtable +0x14
}
