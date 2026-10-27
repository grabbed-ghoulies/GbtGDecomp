#pragma once
#include "strategy_target_queue.h"
#include "strategy_target_selection.h"
#include "strategy_fields.h"

namespace gbtg
{
// entityStrategyGeneric (factory type 0x3C, getter 0x000ABB80, vtable 0x00505D8C) over the base strategy (type 8):
// the plain fighting ghoulie. Holding (0x401, "far") it reselects its target once a second; a target it does not
// abandon sends it attacking (0x3EE: "attack", or by the held weapon "attackprojectile" / "attackgun"), queued on the
// target (0x0002B910), otherwise it closes in (0x40D) and chases (0x3EC, request 0x2F). Hit while unarmed it goes for a
// weapon (0x405, "getweapon") when its goal table has one.
// Strategy bytes: +0x1A attacking, +0x1C pause count, +0x20 target queue link.
namespace generic_strategy_field
{
constexpr RetailField<u32, 0x38> can_arm{}; // Init: the mind's goal table (+0x60) has "getweapon"
constexpr RetailField<u32, 0x3C> reselect{}; // Message 0x6E: the next selection forces a new target
constexpr RetailField<u32, 0x40> attack_new{}; // Message 0x6E without a weapon run: a new target starts an attack
constexpr RetailField<float, 0x44> reselect_timer{}; // 0x000AB900 counts it down by the frame step, wrapping by 1
constexpr u32 kSize = 0x48;
static_assert(retail_fits(kSize, reselect_timer));
} // namespace generic_strategy_field
struct RuntimeGenericStrategy
{
    RuntimeBaseStrategyMessages& base; // Base goals, entry and strategy link (+0x20).
    RuntimeStrategyTargetChecks& checks;
    RuntimeStrategyTargetQueue& queue;
    RuntimeStrategyActorViews& views;
    const char* goal_far; // "far", 0x00321824.
    const char* goal_get_weapon; // "getweapon", 0x00321420.
    const char* goal_attack; // "attack", 0x003211E4.
    const char* goal_attack_projectile; // "attackprojectile", 0x003213AC.
    const char* goal_attack_gun; // "attackgun", 0x003213D4.
    const char* goal_attack_gun_secondary; // "attackgun_secondary", 0x003213C0.
};
// Vtable +0 is 0x000ABB60, haunted_chair_strategy_object_sizes.
bool initialize_runtime_generic_strategy(RuntimeGenericStrategy& context); // Xbox 0x000ABBD0
void dispatch_runtime_generic_message(std::span<const u32> message,
    RuntimeGenericStrategy& context); // Xbox 0x000ABC50, vtable +0xC, through 0x000AB420
void update_runtime_generic_strategy(RuntimeGenericStrategy& context); // Xbox 0x000AB540, vtable +0x10
void enter_runtime_generic_state(u16 state, RuntimeGenericStrategy& context); // Xbox 0x000AB770, vtable +0x14
void leave_runtime_generic_state(u16 state, RuntimeGenericStrategy& context); // Xbox 0x000AB8C0, vtable +0x18
}
