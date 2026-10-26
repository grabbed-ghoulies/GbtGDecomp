#pragma once
#include "strategy_target_queue.h"
#include "strategy_target_selection.h"
#include "strategy_fields.h"
#include "player_goal_cleanup.h"
#include "gbtg/actor_runtime.h"

namespace gbtg
{
// entityStrategyHauntedPicture (factory type 0x43, getter 0x000B0BB0, vtable 0x00505E50) over the base strategy
// (type 8): a haunted picture. Asleep (0x413, "sleep") it wakes (0x417, "wakeup", 0x000B1420) within 50 of its
// target, when its marker key (actor +0x1FC) is clear, or when hit; awake it wanders (0x401, "getrandomdest" and
// a bobbing request 0xF, 0x000B0720) until it has a target, then circles it (0x40C, 0x000B0680) on the movement
// table at 0x00302E80 and attacks (0x3ED "attack" or 0x3EF "attack_special", picked by aggression) between waits
// of 0.45 to 0.55 s.
// Strategy bytes: +0x1A attacking, +0x1B waking, +0x1C pause count, +0x20 target queue link.
namespace haunted_picture_parameter
{
constexpr u32 kSize = 0x18;
} // namespace haunted_picture_parameter
namespace haunted_picture_strategy_field
{
constexpr RetailField<u32, 0x38> awake{}; // set by 0x000B1420
constexpr RetailField<u32, 0x3C> latched{}; // message 0x7D raised the live holds (0x00042090)
constexpr RetailField<float, 0x44> attack_wait{}; // counts down; 0x40C queues on the target once it is 0
constexpr RetailField<float, 0x4C> phase{}; // the bob and circling phase, wrapped at 2.2
constexpr u32 kSize = 0x50;
static_assert(retail_fits(kSize, phase));
} // namespace haunted_picture_strategy_field
struct RuntimeHauntedPictureStrategy
{
    RuntimeBaseStrategyMessages& base; // Base goals, entry and strategy link (+0x20).
    RuntimeStrategyTargetChecks& checks;
    RuntimeStrategyTargetQueue& queue;
    RuntimeStrategyActorViews& views;
    entity_detail::ListHead& live_attachments; // Actor +0x8F4; actor +0x8F8 is the first record.
    RuntimeBodyAttachedActor& self; // 0x00138030's actor.
    RuntimePlayerScriptOverrides& overrides;
    std::span<const u8> movement; // 0x00302E80, 0x34 bytes: the circling ring, its sway and its heading spread.
    float& occupancy_initial; // 0x0013AA50's retained scratch.
    const char* goal_attack; // "attack", 0x003211E4.
    const char* goal_attack_special; // "attack_special", 0x00321738.
    const char* goal_random_dest; // "getrandomdest", 0x00321700.
    const char* goal_sleep; // "sleep", 0x0031D2F8.
    const char* goal_wakeup; // "wakeup", 0x0031D47C.
};
void haunted_picture_strategy_object_sizes(ObjectSizes& output); // Xbox 0x000B0B90
void dispatch_runtime_haunted_picture_message(std::span<const u32> message,
    RuntimeHauntedPictureStrategy& context); // Xbox 0x000B0EA0, vtable +0xC
void update_runtime_haunted_picture_strategy(RuntimeHauntedPictureStrategy& context); // Xbox 0x000B0C00, vtable +0x10
void enter_runtime_haunted_picture_state(u16 state, u16 previous,
    RuntimeHauntedPictureStrategy& context); // Xbox 0x000B1150, vtable +0x14
void leave_runtime_haunted_picture_state(u16 state,
    RuntimeHauntedPictureStrategy& context); // Xbox 0x000B12D0, vtable +0x18
}
