#pragma once
#include <array>
#include "strategy_target_queue.h"
#include "strategy_target_selection.h"
#include "strategy_fields.h"

namespace gbtg
{
// entityStrategyHauntedChair (factory type 0x40, getter 0x000AE720, vtable 0x00505DFC) over the base strategy (type 8):
// scene 8's haunted chairs. A chair holds still for a random time (state 0x401, phase 1), slides somewhere near
// ("slide", phase 2), and when a target is in reach either slides at it ("slideattarget", up to the block's +0x34
// times) or swing-kicks (state 0x3EF), picked by the actor's aggression (+0x964 -> +8). Asleep (0x413, "mimic") it
// waits for a target within the block's reach before waking (0x417, "wakeup").
struct HauntedChairStrategyState
{
    StrategyBase base{}; // +0
    u32 parameters = 0; // +0x38, DAT_00428734, the block the chair's tunables come from
    u32 phase = 0; // +0x3C, 1 hold, 2 slide (0x000AE360)
    float hold = 0.f; // +0x40, seconds left to hold still, drawn from the block's +0x24..+0x28
    i32 slides = 0; // +0x44, slides at the target since state 0x3EE began
};
static_assert(sizeof(HauntedChairStrategyState) == 0x48);
struct RuntimeHauntedChairStrategy
{
    RuntimeBaseStrategyMessages& base; // Base goals, entry and strategy link (+0x20).
    RuntimeStrategyTargetChecks& checks;
    RuntimeStrategyTargetQueue& queue;
    RuntimeStrategyActorViews& views;
    // DAT_00428734: +0 the reach that wakes a sleeping chair, +4 the roam settings 0x0013A0A0 reads, +0x1C / +0x20
    // the slide's width and range (0x00138310), +0x24..+0x28 the hold time's range, +0x34 the slides at a target.
    std::span<const u8> parameters;
    const std::span<const u8>& attributes; // Actor +0x964, the attribute resource; +8 its aggression.
    const char* goal_swing_kick; // "swingkick", 0x0031D494.
    const char* goal_wakeup; // "wakeup", 0x0031D47C.
    const char* goal_slide; // "slide", 0x0032179C.
    const char* goal_slide_at_target; // "slideattarget", 0x0032178C.
    const char* goal_mimic; // "mimic", 0x0031D4C4.
};
void haunted_chair_strategy_object_sizes(ObjectSizes& output); // Xbox 0x000ABB60
bool initialize_runtime_haunted_chair_strategy(std::span<u8> strategy); // Xbox 0x000AE770
// The point `distance` ahead of the actor (along +0x18), at its height.
void runtime_point_ahead_of_actor(const Actor& actor, float distance, Vec3& output); // Xbox 0x00137EF0, EAX actor, EBX output
void update_runtime_haunted_chair_strategy(RuntimeHauntedChairStrategy& context); // Xbox 0x000AE790, vtable +0x10
void dispatch_runtime_haunted_chair_message(std::span<const u32> message,
    RuntimeHauntedChairStrategy& context); // Xbox 0x000AE8B0, vtable +0xC
void enter_runtime_haunted_chair_state(u16 state, u16 previous,
    RuntimeHauntedChairStrategy& context); // Xbox 0x000AEA10, vtable +0x14
void leave_runtime_haunted_chair_state(u16 state, u16 next,
    RuntimeHauntedChairStrategy& context); // Xbox 0x000AEB30, vtable +0x18
}
