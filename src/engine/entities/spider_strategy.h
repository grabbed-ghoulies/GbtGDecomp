#pragma once
#include "strategy_target_queue.h"
#include "strategy_target_selection.h"
#include "strategy_fields.h"

namespace gbtg
{
// entityStrategySpider (factory type 0x52, getter 0x000C0B60, vtable 0x00505FF4) over the base strategy (type 8). It
// keeps the skeleton's sizes (0x000A9FC0, 0x40 bytes), the shared constructor 0x000CFFD0 and the empty destructor
// 0x001628B0, and shares its leave slot 0x000B5AF0. A spider flanks its target 30 units out, then bites, or spits
// sticky web once it is near; a hit on it holds the web 5 s (+0x38).
// Strategy bytes: +0x1A biting, +0x1B spitting, +0x1C pause count, +0x20 target queue link.
namespace spider_strategy_field
{
constexpr RetailField<float, 0x38> web_delay{}; // DAT_0032B184 after a hit on itself; no web while it runs
constexpr RetailField<u32, 0x3C> reached{}; // 0x000C0620's result in states 0x3ED and 0x3EF
} // namespace spider_strategy_field
struct RuntimeSpiderStrategy
{
    RuntimeBaseStrategyMessages& base; // Base goals, entry and strategy link (+0x20).
    RuntimeStrategyTargetChecks& checks;
    RuntimeStrategyTargetQueue& queue;
    RuntimeStrategyActorViews& views;
    const char* goal_attack; // "attack", 0x003211E4.
    const char* goal_attack_now; // "attack_nodelay", 0x00321344.
    const char* goal_web; // "stickywebspit", 0x00321334.
};
// The target climbs (its body's +0x874, or +0x87E while +0x880 is set, is 12).
bool runtime_mind_target_climbing(const RuntimeMindTarget& target,
    RuntimeSpiderStrategy& context); // Xbox 0x00141490, EAX target
// The gap from the actor's reach (actor +0x38 by body +0x888, plus the target's +0x38) to the target, at least 0.
float runtime_strategy_target_gap(const RuntimeMindTarget& target,
    RuntimeSpiderStrategy& context); // Xbox 0x00141650, EAX actor, EBX target
// Mind requests 0x2D to the point 30 units to one side of the target (+0x70, both sides tried) and 0x29.
bool approach_runtime_spider_target(const RuntimeMindTarget& target,
    RuntimeSpiderStrategy& context); // Xbox 0x000C0620, EAX strategy
// Mind requests 0x2D to one of four clear points 30 units around the spider (else one 40 out) and 0x29.
void roam_runtime_spider(RuntimeSpiderStrategy& context); // Xbox 0x000C08A0, EAX strategy
// The pending requests are dropped once the goal point (+0x70) lies in a cell kind past the spider's own.
void check_runtime_spider_goal_cell(RuntimeSpiderStrategy& context); // Xbox 0x000C0AF0, EAX strategy
bool runtime_spider_holds_target(const RuntimeMindTarget& target,
    RuntimeSpiderStrategy& context); // Xbox 0x000C1150, EAX target, EBX strategy
bool runtime_spider_bites(const RuntimeMindTarget* target,
    RuntimeSpiderStrategy& context); // Xbox 0x000C1190, EAX strategy, ESI target
void choose_runtime_spider_attack(const RuntimeMindTarget& target,
    RuntimeSpiderStrategy& context); // Xbox 0x000C11F0, EAX target, EDI strategy
void end_runtime_spider_web(const RuntimeMindTarget& target,
    RuntimeSpiderStrategy& context); // Xbox 0x000C1290, EAX body, ECX target, EDI strategy
void update_runtime_spider_strategy(RuntimeSpiderStrategy& context); // Xbox 0x000C0BB0, vtable +0x10
void dispatch_runtime_spider_message(std::span<const u32> message,
    RuntimeSpiderStrategy& context); // Xbox 0x000C0F00, vtable +0xC
void enter_runtime_spider_state(u16 state, u16 previous,
    RuntimeSpiderStrategy& context); // Xbox 0x000C1040, vtable +0x14
void leave_runtime_spider_state(u16 state, RuntimeSpiderStrategy& context); // Xbox 0x000B5AF0, vtable +0x18
}
