#pragma once
#include <array>
#include "strategy_fields.h"
#include "spider_strategy.h"

namespace gbtg
{
// entityStrategyImpFlying (factory type 0x48, getter 0x000B6B70, vtable 0x00505EDC; the factory table's row 0x47
// carries the name) over the base strategy (type 8): the flying imp. It hovers to clear points 30 units around itself
// at a random height ("evade" when its target is near), whacks a queued target with its wings ("wingwhackprepare",
// state 0x3ED) or arrow-dives at it ("arrowdive", 0x3EF): the dive's effect, a hold, then a lunge to its model's
// marker 0 and a climb back ("arrowdiveunstuck"). A hit sets it down (0x409), and state 0x417 flies to its hover
// height.
struct ImpFlyingStrategyState
{
    StrategyBase base{}; // +0
    // +0x38: DAT_00303124: +0 near reach, +4 far reach, +8 hover height, +0xC climb height, +0x10 the dive's hold
    // seconds, +0x24 its lunge seconds.
    u32 parameters = 0;
    std::array<char, 0x80> effect_name{}; // +0x3C, the dive's effect template, copied from parameters +0x18
    u32 effect = 0; // +0xBC, the dive's effect object
    u32 dive_phase = 0; // +0xC0, state 0x3EF: 1 dive, 2 hold, 3 lunge and climb
    float hold = 0.f; // +0xC4, seconds left in dive phase 2
    u32 lunging = 0; // +0xC8, dive phase 3 has taken its start and end points
    Vec3 lunge_end{}; // +0xCC, the model's marker 0 in the world (0x0003AE60)
    Vec3 lunge_start{}; // +0xD8, the actor's position when the lunge began
    float lunge = 0.f; // +0xE4, seconds left in the lunge, from parameters +0x24
    u32 whack_step = 0; // +0xE8, state 0x3ED: 1 preparing, 2 evading
    u32 landing_step = 0; // +0xEC, state 0x409: 1 waits for body bit 0x31, 2 rises
    float rise = 0.f; // +0xF0, state 0x417's seconds left
    u32 high = 0; // +0xF4, the next hover height is drawn from the higher bands (0x000B6A30)
};
static_assert(sizeof(ImpFlyingStrategyState) == 0xF8);
struct RuntimeImpFlyingStrategy
{
    // The instance's spider views over the same strategy: base goals, target checks, queue and actor views.
    RuntimeSpiderStrategy& shared;
    std::span<const u8> parameters; // DAT_00303124, 0x28 bytes.
    std::function<u32(const char*, u32)> create_effect; // 0x0003DF40, EAX the name, the actor pushed.
    std::function<void(u32, std::span<const u32>)> object_message; // An object's vtable +0xC.
    const char* goal_evade; // "evade", 0x003217D8.
    const char* goal_wing_whack_prepare; // "wingwhackprepare", 0x003215FC.
    const char* goal_arrow_dive; // "arrowdive", 0x00321630.
    const char* goal_arrow_dive_unstuck; // "arrowdiveunstuck", 0x0032161C.
};
void imp_flying_strategy_object_sizes(ObjectSizes& output); // Xbox 0x000B6B50
bool initialize_runtime_imp_flying_strategy(std::span<u8> strategy,
    std::span<const u8> parameters); // Xbox 0x000B6BC0
void update_runtime_imp_flying_strategy(RuntimeImpFlyingStrategy& context); // Xbox 0x000B6BF0, vtable +0x10
void dispatch_runtime_imp_flying_message(std::span<const u32> message,
    RuntimeImpFlyingStrategy& context); // Xbox 0x000B6CB0, vtable +0xC
void enter_runtime_imp_flying_state(u16 state, u16 previous,
    RuntimeImpFlyingStrategy& context); // Xbox 0x000B6F80, vtable +0x14
void leave_runtime_imp_flying_state(u16 state, RuntimeImpFlyingStrategy& context); // Xbox 0x000B7050, vtable +0x18
}
