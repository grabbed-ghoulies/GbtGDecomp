#pragma once
#include <array>
#include "strategy_target_queue.h"
#include "strategy_target_selection.h"
#include "strategy_fields.h"
#include "body_action_selection.h"
#include "player_attachment_helpers.h"
#include "player_goal_cleanup.h"
#include "gbtg/actor_runtime.h"

namespace gbtg
{
// entityStrategyHauntedCoat (factory type 0x41, getter 0x000AF050, vtable 0x00505E18) over the base strategy (type 8):
// a haunted coat. It hangs on the wall when it starts 3 above its floor cell (+0x38), sleeping (0x413, "sleep" or
// "sleep_wall") until a target comes within 60 or hits it; it wakes (0x417, "wakeup" or "wakeup_wall") and waits
// (0x416), then closes in (0x40C), swaying on a heading search beside its target, and attacks (0x3ED, "attack") once
// the target stands near and in front.
namespace haunted_coat_parameter
{
constexpr u32 kSize = 0x18;
} // namespace haunted_coat_parameter
struct HauntedCoatStrategyState
{
    StrategyBase base{}; // +0
    u32 on_wall = 0; // +0x38, init: the actor stands 3 above its floor cell (DAT_0032B260)
    u32 awake = 0; // +0x3C, set by 0x000AF870
    u32 latched = 0; // +0x40, message 0x7D raised the live holds (0x00042090)
    float sway = 0.f; // +0x44, 0x000AEBF0's step, wrapped at 2
    float closing = 0.f; // +0x48, 0x000AEBF0's step since entering 0x3ED; attack once above 1
    float turn = 0.f; // +0x4C, 0x000AEBF0's step, held at 1: the heading's turn fraction
    float bob = 0.f; // +0x50, 0x000AEBF0's step, wrapped at 2.2
};
static_assert(sizeof(HauntedCoatStrategyState) == 0x54);
struct RuntimeHauntedCoatStrategy
{
    RuntimeBaseStrategyMessages& base; // Base goals, entry and strategy link (+0x20).
    RuntimeStrategyTargetChecks& checks;
    RuntimeStrategyTargetQueue& queue;
    RuntimeStrategyActorViews& views;
    RuntimeBodyActionSelection& actions; // The inlined 0x00027650.
    entity_detail::ListHead& live_attachments; // Actor +0x8F4; actor +0x8F8 is the first record.
    RuntimeBodyAttachedActor& self; // 0x00138030's actor.
    RuntimePlayerScriptOverrides& overrides;
    float& occupancy_initial; // 0x0013AA50's retained scratch.
    std::function<const EntityBitBlock&()> bits; // Body +0x15C -> +8.
    std::function<bool(std::span<const u8>)> point_is_near; // 0x00134350 from the body to a target record.
    std::function<void(std::span<const u32>)> body_message; // Body vtable +0xC.
    const char* goal_attack; // "attack", 0x003211E4.
    const char* goal_sleep; // "sleep", 0x0031D2F8.
    const char* goal_sleep_wall; // "sleep_wall", 0x00321754.
    const char* goal_wakeup; // "wakeup", 0x0031D47C.
    const char* goal_wakeup_wall; // "wakeup_wall", 0x00321748.
};
void haunted_coat_strategy_object_sizes(ObjectSizes& output); // Xbox 0x000AF030
bool initialize_runtime_haunted_coat_strategy(RuntimeHauntedCoatStrategy& context); // Xbox 0x000AF0A0
void dispatch_runtime_haunted_coat_message(std::span<const u32> message,
    RuntimeHauntedCoatStrategy& context); // Xbox 0x000AF340, vtable +0xC
void update_runtime_haunted_coat_strategy(RuntimeHauntedCoatStrategy& context); // Xbox 0x000AF180, vtable +0x10
void enter_runtime_haunted_coat_state(u16 state, u16 previous,
    RuntimeHauntedCoatStrategy& context); // Xbox 0x000AF5A0, vtable +0x14
void leave_runtime_haunted_coat_state(u16 state, RuntimeHauntedCoatStrategy& context); // Xbox 0x000AF6E0, vtable +0x18
}
