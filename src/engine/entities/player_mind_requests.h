#pragma once
#include "mind_action_requests.h"
#include "body_attachment_requests.h"
#include "body_animation_commands.h"
#include "player_mind_feedback.h"
#include "actor_spawn_query.h"
#include "player_messages.h"
#include "../scripts/script_actor_query.h"

namespace gbtg
{
bool notify_runtime_player_boost(RuntimeMindBoost& context,
    RuntimePlayerMindFeedback& feedback); // Xbox 0x0007F8C0
bool notify_runtime_player_turn(RuntimeMindBoost& context,
    RuntimePlayerMindFeedback& feedback, const Vec3& point); // Xbox 0x0007F920
u32 runtime_player_callout_name_id(const char* name,
    const char* const* names); // Xbox 0x0003BFA0
bool runtime_actor_animation_slot_complete(const ActorAnimationState& animation,
    u32 slot); // Xbox 0x0003B290
bool runtime_actor_animation_crossed_position(const ActorAnimationState& animation,
    u32 slot, float position); // Xbox 0x0003B1F0
float runtime_player_target_radius(std::span<const u8> body, const Actor& body_actor,
    const RuntimeBodyAttachedActor* attached, const Actor& target); // Xbox 0x001416D0
Actor* find_runtime_player_nearest_spawn(const Actor& actor, float* distance,
    u8 flags); // Xbox 0x00129C20
struct RuntimeParentMindRequests
{
    RuntimeMindEvents& events;
    RuntimeMindGoalContext& goals;
    RuntimeBodyAttachmentRequests& attachment;
    Actor& actor;
    Actor& body_actor;
    EntityBitBlock& flags;
    RuntimeScriptMindQuery& body_query;
    ActorAnimationState& animation;
    RuntimeActorResourceReference& resource;
    ActorAttachmentEventContext& callout;
    std::span<const char* const, 0x41> flag_names;
    const char* const* action_names; // 0x174 names.
    const char* const* callout_names; // 0x55 names.
    const char* const* goal_flag_names; // 3 names.
    u32& random;
    std::function<void(std::span<u32>)> actor_message; // Actual actor vtable +0xC.
    std::function<u16()> body_effect_key; // Actual body vtable +0x28.
    std::function<u32(const Actor*)> actor_address; // Retail actor token mapping.
    std::span<const u8> target_bytes; // Mind +0x10.
    const Actor* target_actor;
};
i32 update_runtime_parent_mind_request(RuntimeParentMindRequests& context,
    std::span<const u8> request); // Xbox 0x000295E0
void notify_runtime_player_mind_attachment_callout(RuntimeParentMindRequests& context,
    const std::span<const u8>* action_target, u32 sender); // Xbox 0x0007DD30
}
