#pragma once
#include "player_strategy_entry.h"
#include "player_goal_cleanup.h"
#include "player_messages.h"
#include "player_attachment.h"
#include "player_parent_lifecycle.h"
#include "../scripts/script_actor_query.h"
#include "../graphics/actor_effect_stream.h"
#include "../frontend/input_activation.h"
#include <unordered_map>

namespace gbtg
{
struct RuntimePlayerStrategyGoals
{
    // Strategy bytes, actor, body, mind events and goal loading, goal ring, actor +0xCA4 and
    // the target registry; goal_default is "celebrate" (0x0031B548).
    RuntimePlayerStrategyEntry& entry;
    FactoryObject& strategy;
    RuntimePlayerStrategySetup& setup; // Strategy vtable +0x14/+0x18 and mind requests (mind +0x58).
    RuntimePlayerGoalCleanup& cleanup; // 0x00028240; first is DAT_00511AA4.
    const RuntimePlayerStrategyResume& resume; // DAT_0050E764/0x0050E768 and decay table.
    EntityBitBlock*& body_bits; // Body +0x15C.
    RuntimeBodyActionSelection& actions; // Body +0x48, +0x73C and action selection.
    std::function<u16()> primary_action_key; // Passed to 0x00135700.
    Game& game;
    std::function<u16(u32)> body_action_key; // Actual body vtable slot at the given byte offset.
    std::function<RuntimeMindTarget*(u32)> target_at; // Native target behind a retail address.
    std::function<u32(const void*)> address_of; // Retail address of a native record.
    RuntimePlayerAttachmentActor& pose; // Actor pose state for 0x0003AE00.
    const std::unordered_map<RuntimeBodyAttachedActor*, RuntimePlayerAttachmentActor*>& attached_poses;
    ActorEffectStreamContext& effects;
    std::span<u8> input; // Actor +0x960.
    const RuntimeInputReleaseContext& input_release;
    const OccupancyRecord*& room; // Actor +0xCA0.
    const OccupancyRecord*& current; // Actor +0xCA8.
    const OccupancyRecord*& goal_volume; // Actor +0x98C.
    const void* const& script_records; // DAT_0051108C.
    const u32& runtime_flags; // DAT_0050EA9C.
    u32& random; // DAT_0054DD7C.
    const RuntimeActorClock& clock;
    const char* goal_intro; // "ghouly_intro", 0x00321808.
    const char* goal_wait_for_scare; // "drone_waitforscare", 0x00321828.
};

void record_runtime_player_free_position(RuntimePlayerStrategyGoals& context); // Xbox 0x0002B540, EAX actor
u32 tick_runtime_player_goal_delay(RuntimePlayerStrategyGoals& context); // Xbox 0x000285B0, EAX actor
void clear_runtime_player_goal_volume(RuntimePlayerStrategyGoals& context); // Xbox 0x00028580, ESI actor
void set_runtime_player_goal_volume(const OccupancyRecord* volume,
    RuntimePlayerStrategyGoals& context); // Xbox 0x00028500, EAX volume, ESI actor
const OccupancyRecord* find_runtime_scripted_occupancy(i16 value,
    const void* records); // Xbox 0x00124800
void release_runtime_actor_controller(ActorAnimationState& animation, bool send_end,
    AnimationResourceRegistry& registry); // Xbox 0x0003AC90, EDI actor
void set_runtime_mind_alert_event(RuntimeMindEvents& events, u32 target); // Xbox 0x0002A080, EAX mind, ESI target
void load_runtime_goal_effect(const char* name, const Vec3& position, const Vec3& euler,
    ActorEffectStreamContext& effects); // Xbox 0x0013E6B0, ECX position, EAX euler
void play_runtime_player_goal_effects(RuntimePlayerStrategyGoals& context); // Xbox 0x00028150, EAX actor
void alert_runtime_goal_listener(RuntimePlayerStrategyGoals& context); // Xbox 0x0013D400
u32 runtime_player_goal_ready(RuntimePlayerStrategyGoals& context); // Xbox 0x00135420, ESI actor
bool find_runtime_goal_volume_target(const OccupancyRecord& volume, u32& output,
    RuntimePlayerStrategyGoals& context); // Xbox 0x00139E90, ECX volume, EAX strategy
u32 seek_runtime_player_free_position(RuntimePlayerStrategyGoals& context); // Xbox 0x0013A0F0, EAX strategy
u32 select_runtime_body_slot_action(u32 slot, RuntimePlayerStrategyGoals& context); // Xbox 0x00135B40, EAX body
u32 select_runtime_carry_action(RuntimePlayerStrategyGoals& context); // Xbox 0x00135AB0, ESI body
u32 select_runtime_goal_kind_action(u16 kind, u32& output,
    RuntimePlayerStrategyGoals& context); // Xbox 0x00135790, AX kind, ECX actor
u32 select_runtime_player_goal_action(u32& output,
    RuntimePlayerStrategyGoals& context); // Xbox 0x00135540, EAX actor, ESI output
u32 enter_runtime_player_goal_action(RuntimePlayerStrategyGoals& context); // Xbox 0x001391D0, EAX actor, ESI strategy
u32 begin_runtime_player_goal(RuntimePlayerStrategyGoals& context); // Xbox 0x00138F60, EDI strategy
u32 complete_runtime_player_goal_step(RuntimePlayerStrategyGoals& context); // Xbox 0x00139010, EAX actor, ECX strategy
u32 update_runtime_player_goal_state(RuntimePlayerStrategyGoals& context); // Xbox 0x00138A30, EBX strategy
void update_runtime_base_strategy(RuntimePlayerStrategyGoals& context); // Xbox 0x0002C130

// Native view of an actor named by a strategy message word.
struct RuntimeStrategyMessageActor
{
    u32 address;
    Actor* actor;
    std::span<const u8> bytes; // +0x1FC and +0x1FE words.
    RuntimeMindTarget* target; // +0x8F8 registry record; its +0x18 byte disables it.
    RuntimeStrategyMessageActor* holder; // Body +0x274.
};
struct RuntimeBaseStrategyMessages
{
    RuntimePlayerStrategyGoals& goals;
    const RuntimeScriptActorQueryContext& query; // 0x00028360.
    RuntimeStrategyLink& link; // Strategy +0x20.
    std::function<RuntimeStrategyMessageActor*(u32)> message_actor;
    const u8*& carried; // Body +0x268 object; byte +0x199.
};
u32 runtime_player_mind_target(RuntimePlayerStrategyGoals& context); // Xbox 0x00028C30, EAX actor
RuntimeMindTarget* runtime_actor_active_target(const RuntimeStrategyMessageActor& actor); // Xbox 0x00042070, EAX actor
void append_runtime_player_goal(u16 kind, u32 value, std::span<const u32, 3> position,
    std::span<const u32, 3> second, RuntimePlayerStrategyGoals& context); // Xbox 0x00028420, ESI actor, EBX/EDI vectors
u32 queue_runtime_player_goal(std::span<const u32> message,
    RuntimePlayerStrategyGoals& context); // Xbox 0x001387F0, EDX message
void resolve_runtime_goal_alert(RuntimePlayerStrategyGoals& context); // Xbox 0x00138C80
void deliver_runtime_goal_item(std::span<const u32> message, RuntimeBaseStrategyMessages& context); // Xbox 0x00138B40
void complete_runtime_goal_contact(u32 first, u32 second,
    RuntimeBaseStrategyMessages& context); // Xbox 0x001392D0, EAX first, EBX second
void complete_runtime_goal_handoff(std::span<const u32> message,
    RuntimeBaseStrategyMessages& context); // Xbox 0x00138E40, EDX message
void complete_runtime_goal_script(std::span<const u32> message,
    RuntimeBaseStrategyMessages& context); // Xbox 0x00138EF0, EDI strategy
void dispatch_runtime_base_strategy_message(std::span<const u32> message,
    RuntimeBaseStrategyMessages& context); // Xbox 0x0002BC10
}
