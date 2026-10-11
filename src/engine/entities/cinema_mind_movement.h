#pragma once
#include "player_attachment_helpers.h"
#include "mind_target_policy.h"
#include "body_request_selection.h"
#include "../frontend/input_activation.h"
#include "../navigation/player_navigation.h"
#include "../navigation/mind_escape_position.h"

namespace gbtg
{
struct RuntimeBodyDamageTables;
struct RuntimeCinemaMindMovement
{
    std::span<u8> mind;
    std::span<const u8> actor_bytes;
    std::span<const u8> body;
    std::span<const u8> body_actor_bytes;
    Actor& actor;
    const RuntimePlayerAttachmentEligibility& eligibility;
    RuntimePlayerNavigationControl& navigation;
    RuntimeMindEscapePosition& escape;
    const RuntimeActorClock& clock;
    const OccupancyRecord* required; // Actor +0xCA0.
    const OccupancyRecord* allowed; // Actor +0xCA4.
    u32& random;
    float occupancy_initial;
    RuntimeMindTarget* target; // Native mind +0x10.
    RuntimeScriptMindQuery& body_query;
};
u32 update_runtime_cinema_mind_vertical_target(RuntimeCinemaMindMovement& context,
    std::span<const u8> request); // Xbox 0x0007B620
u32 update_runtime_cinema_mind_target_position(RuntimeCinemaMindMovement& context,
    std::span<const u8> request); // Xbox 0x0007BC80
bool update_runtime_cinema_mind_target_action(RuntimeCinemaMindMovement& context,
    std::span<const u8> request, RuntimeMindChainRequestSelection& selection,
    std::span<u8> input, const RuntimeInputReleaseContext& input_context); // Xbox 0x0007B090
u32 request_runtime_cinema_mind_spawn_attachment(RuntimeCinemaMindMovement& context,
    RuntimeBodyAttachedActor& target, std::span<const u8> target_link,
    const std::function<void(RuntimeBodyAttachedActor&)>& attach,
    std::span<u8> body, RuntimeBodyActionSelection& actions, u16 action_key); // Xbox 0x0007B840
u32 update_runtime_cinema_mind_timed_motion(RuntimeCinemaMindMovement& context,
    std::span<const u8> request, const RuntimeBodyDamageTables& tables,
    std::span<const float> mode_scales); // Xbox 0x0007BA20
struct RuntimeCinemaMindSpawnMovement
{
    RuntimeCinemaMindMovement& movement;
    RuntimeBodyAttachedActor*& target; // Native mind +0x18.
    const std::unordered_map<Actor*, RuntimeBodyAttachedActor*>& actors;
    std::function<std::span<const u8>(const RuntimeBodyAttachedActor&)> target_link;
};
u32 update_runtime_cinema_mind_spawn_position(RuntimeCinemaMindSpawnMovement& context); // Xbox 0x0007BF60
u32 update_runtime_cinema_mind_escape_motion(RuntimeCinemaMindMovement& context,
    std::span<u8> request); // Xbox 0x0007B170
u32 update_runtime_cinema_mind_vertical_escape_motion(RuntimeCinemaMindMovement& context,
    std::span<u8> request); // Xbox 0x0007B2F0
u32 update_runtime_cinema_mind_vertical_position(RuntimeCinemaMindMovement& context,
    std::span<const u8> request); // Xbox 0x0007B5A0
u32 update_runtime_mind_vertical_position_request(RuntimeCinemaMindMovement& context,
    std::span<const u8> request); // Xbox 0x00136680
u32 update_runtime_cinema_mind_position(RuntimeCinemaMindMovement& context,
    std::span<const u8> request); // Xbox 0x0007BC20
u32 select_runtime_cinema_mind_random_position(RuntimeCinemaMindMovement& context,
    std::span<const u8> request); // Xbox 0x0007B8A0
u32 select_runtime_cinema_mind_heading_position(RuntimeCinemaMindMovement& context,
    std::span<const u8> request); // Xbox 0x0007B940
}
