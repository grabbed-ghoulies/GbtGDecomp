#pragma once
#include "strategy_target_queue.h"
#include "strategy_target_selection.h"
#include "strategy_fields.h"
#include "body_action_selection.h"
#include "body_attachment_requests.h"
#include "body_resources.h"
#include "player_goal_cleanup.h"
#include "gbtg/actor_runtime.h"
#include "gbtg/resource_error.h"
#include <unordered_map>

namespace gbtg
{
// entityStrategyHauntedDoor (factory type 0x42, getter 0x000AFD30, vtable 0x00505E34) over the base strategy (type 8):
// a haunted door. Asleep (0x413, "sleep") it wakes (0x417, "wakeup") once a target comes within 60; awake it queues
// on its target and knocks it down (0x3EE, "knockdown") or attacks (0x3EF, "attack_special"), picked by the actor's
// aggression. While it knocks, its knob attack overlay ("knobattack_overlay") replays on a timeline of waits (the
// parameters' AID, +0x5C..+0x68) whose waits stretch by DAT_0032B398 per hit taken (+0x48).
// Strategy bytes: +0x1A attacking, +0x1B waking, +0x1C pause count, +0x20 target queue link.
namespace haunted_door_parameter
{
constexpr RetailField<std::array<char, 0x80>, 0x18> timeline{}; // the knob attack's wait timeline AID
constexpr u32 kSize = 0x98;
static_assert(retail_fits(kSize, timeline));
} // namespace haunted_door_parameter
namespace haunted_door_strategy_field
{
constexpr RetailField<u32, 0x38> awake{}; // set on waking (0x417); message 0x31 and hits react only then
constexpr RetailField<u32, 0x3C> latched{}; // message 0x7D raised the body latches (0x0001FC40)
constexpr RetailField<u32, 0x40> close_in{}; // state 0x40A's draw (0x00019200, 0.5): attack once within 40
constexpr RetailField<float, 0x44> wait{}; // state 0x416's countdown from 1 s
constexpr RetailField<i32, 0x48> hits{}; // knock-downs taken (message 0x6E)
constexpr RetailField<u32, 0x4C> overlay{}; // set when the overlay's animation record was found
constexpr RetailField<u32, 0x50> overlay_on{}; // the overlay runs (0x000AFA80)
constexpr RetailField<u32, 0x54> attacking{}; // the overlay's attack row is on (0x000AFB20 .. 0x000AFB90)
constexpr RetailField<u32, 0x58> overlay_wanted{}; // the overlay starts once the body settles
constexpr RetailField<u32, 0x5C> timeline{}; // the timeline's floats, or 0
constexpr RetailField<u32, 0x60> timeline_end{};
constexpr RetailField<u32, 0x64> timeline_row{};
constexpr RetailField<float, 0x68> row_time{};
constexpr u32 kSize = 0x6C;
static_assert(retail_fits(kSize, row_time));
} // namespace haunted_door_strategy_field
struct RuntimeHauntedDoorStrategy
{
    RuntimeBaseStrategyMessages& base; // Base goals, entry and strategy link (+0x20).
    RuntimeStrategyTargetChecks& checks;
    RuntimeStrategyTargetQueue& queue;
    RuntimeStrategyActorViews& views;
    RuntimeBodyActionSelection& actions; // 0x0001AED0's lookup and 0x0001AFA0's start.
    AnimationResourceRegistry& registry;
    ResourceErrorState& error; // 0x00032700.
    std::span<const RuntimeBodyLoadedResourceSize> loaded_sizes; // 0x00035D90's sizes.
    std::span<const u8>& attack_resource; // Body +0x5C.
    const std::unordered_map<const u8*, const RuntimeAttachmentRequestTables*>& attack_tables;
    const std::span<const u8>& attributes; // Actor +0x964, the attribute resource; +8 its aggression.
    entity_detail::LatchNode*& latches; // DAT_00511AA4.
    entity_detail::ListHead& live_attachments; // Actor +0x8F4; actor +0x8F8 is the first record.
    RuntimeBodyAttachedActor& self; // 0x00138030's actor.
    RuntimePlayerScriptOverrides& overrides;
    std::function<u32(const void*)> address_of; // Retail address of a native object or resource.
    std::function<float(std::span<const u8>)> surface_distance; // 0x0013A5B0 from this actor to a target record.
    // 0x0003C040's records on the actor at an address, with the creation options.
    std::function<void(u32, i32, const RuntimeActorCreationOptions*)> actor_callout;
    const char* overlay_name; // "knobattack_overlay", 0x0031D3C0.
    const char* goal_knockdown; // "knockdown", 0x003217C0.
    const char* goal_attack_special; // "attack_special", 0x00321738.
    const char* goal_knocked_down; // "knocked_down", 0x003217F8.
    const char* goal_knocked_down_wait; // "knocked_down_wait", 0x00321710.
    const char* goal_sleep; // "sleep", 0x0031D2F8.
    const char* goal_wakeup; // "wakeup", 0x0031D47C.
    const char* goal_celebrate; // "celebrate_knockdown", 0x00321724.
};
void haunted_door_strategy_object_sizes(ObjectSizes& output); // Xbox 0x000AFD10
// The overlay's record and the wait timeline (0x000AF920).
bool initialize_runtime_haunted_door_strategy(std::span<const u8> parameters,
    RuntimeHauntedDoorStrategy& context); // Xbox 0x000AFD80
void destroy_runtime_haunted_door_strategy(RuntimeHauntedDoorStrategy& context); // Xbox 0x000AFDA0
void update_runtime_haunted_door_strategy(RuntimeHauntedDoorStrategy& context); // Xbox 0x000AFDD0, vtable +0x10
void dispatch_runtime_haunted_door_message(std::span<const u32> message,
    RuntimeHauntedDoorStrategy& context); // Xbox 0x000B0140, vtable +0xC
void enter_runtime_haunted_door_state(u16 state, u16 previous,
    RuntimeHauntedDoorStrategy& context); // Xbox 0x000B0340, vtable +0x14
void leave_runtime_haunted_door_state(u16 state, RuntimeHauntedDoorStrategy& context); // Xbox 0x000B0520, vtable +0x18
// While the overlay runs: stopped when the body moves, otherwise replayed at each timeline step with the target in
// sight; a wanted overlay starts once the body settles.
void drive_runtime_haunted_door_overlay(RuntimeHauntedDoorStrategy& context); // Xbox 0x000AF9D0, EAX strategy
void start_runtime_haunted_door_overlay(RuntimeHauntedDoorStrategy& context); // Xbox 0x000AFA80, EAX strategy
// Message 0x41: the hit actor's callout 0x34 at the message's point (+0x1C).
void call_out_runtime_haunted_door_hit(std::span<const u32> message,
    RuntimeHauntedDoorStrategy& context); // Xbox 0x000AFAC0, EAX message
void begin_runtime_haunted_door_attack(RuntimeHauntedDoorStrategy& context); // Xbox 0x000AFB20, EDI strategy
void end_runtime_haunted_door_attack(RuntimeHauntedDoorStrategy& context); // Xbox 0x000AFB90, EBX strategy
bool runtime_haunted_door_sees_target(RuntimeHauntedDoorStrategy& context); // Xbox 0x000AFC00, EAX strategy
// The timeline's wait (stretched by the hits) runs out: true, and the next row is current.
bool step_runtime_haunted_door_timeline(RuntimeHauntedDoorStrategy& context); // Xbox 0x000AFC80, ESI strategy, EBX actor
// Knock down (0x3EE) or, when the aggression roll allows, attack (0x3EF).
void choose_runtime_haunted_door_attack(RuntimeHauntedDoorStrategy& context); // Xbox 0x000B05B0, ESI strategy
}
