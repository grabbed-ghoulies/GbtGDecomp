#pragma once
#include "spider_strategy.h"

namespace gbtg
{
// Factory type 0x47 (getter 0x000B5520, vtable 0x00505EC0) over the base strategy (type 8); the factory table names it
// entityStrategyImpFire. It keeps the director's sizes (0x000AAE40, 0x44 / 0x18 bytes), the shared constructor
// 0x000CFFD0, the empty destructor 0x001628B0 and the spider's leave slot 0x000B5AF0. A flying imp: it roams to clear
// points 30 units out, closes on its target to one side (25 degrees off) and, queued on it, throws a fireball
// ("fireball", state 0x3ED) or, by its aggression, breathes fire ("flamethrower", 0x3EF).
// Strategy bytes: +0x1A attacking, +0x1C pause count, +0x20 target queue link.
namespace imp_flamethrower_strategy_field
{
constexpr RetailField<float, 0x38> delay{}; // the roam or wait countdown (states 0x401 and 0x40D)
constexpr RetailField<u32, 0x3C> fast{}; // the moves' +0x18 is 25 rather than 5
constexpr RetailField<u32, 0x40> in_reach{}; // state 0x3ED's approach ended within 45 of the target
constexpr u32 kSize = 0x44;
static_assert(retail_fits(kSize, in_reach));
} // namespace imp_flamethrower_strategy_field
struct RuntimeImpFlamethrowerStrategy
{
    // The instance's spider views over the same strategy: 0x000C0AF0, 0x00141650 and the shared leave 0x000B5AF0.
    RuntimeSpiderStrategy& shared;
    const char* goal_fireball; // "fireball", 0x0031D148.
    const char* goal_flamethrower; // "flamethrower", 0x0032163C.
    const char* goal_wait; // "wait", 0x0031CB4C.
};
// Mind requests 0xF to a clear point 30 units to one side of the target (+0x19 its floor), else the target itself,
// and 0x29; true when the target's gap is within 45.
bool approach_runtime_imp_flamethrower_target(const RuntimeMindTarget* target,
    RuntimeImpFlamethrowerStrategy& context); // Xbox 0x000B4FE0
// Mind requests 0xF to one of four clear points 30 units around the imp and 0x29; nothing when none is clear.
void roam_runtime_imp_flamethrower(RuntimeImpFlamethrowerStrategy& context); // Xbox 0x000B52B0
// Fireball (0x3ED) or, when the aggression roll allows, the flamethrower (0x3EF).
void choose_runtime_imp_flamethrower_attack(RuntimeImpFlamethrowerStrategy& context); // Xbox 0x000B1350, ESI strategy
void update_runtime_imp_flamethrower_strategy(RuntimeImpFlamethrowerStrategy& context); // Xbox 0x000B5570, vtable +0x10
void dispatch_runtime_imp_flamethrower_message(std::span<const u32> message,
    RuntimeImpFlamethrowerStrategy& context); // Xbox 0x000B5860, vtable +0xC
void enter_runtime_imp_flamethrower_state(u16 state, u16 previous,
    RuntimeImpFlamethrowerStrategy& context); // Xbox 0x000B58F0, vtable +0x14
}
