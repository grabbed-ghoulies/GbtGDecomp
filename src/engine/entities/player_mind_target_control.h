#pragma once
#include "player_mind_targets.h"
#include "player_mind_control.h"
#include "player_mind_feedback.h"

namespace gbtg
{
struct RuntimePlayerTargetView
{
    std::span<const u8> bytes;
    u32 address;
    Actor* owner; // Target +0x10.
    RuntimeMindTarget* searchable;
};
struct RuntimePlayerMindProposal
{
    RuntimePlayerTargetSearch& search;
    RuntimePlayerMindShapeTarget& shape;
    RuntimeMindEvents& events;
    const std::unordered_map<u32, RuntimePlayerTargetView*>& targets;
    RuntimePlayerTargetView& embedded; // Mind +0xB8.
};
RuntimePlayerTargetView* update_runtime_player_target_proposal(RuntimePlayerMindProposal& context,
    u32 enabled, float heading); // Xbox 0x0007E880
u32 select_runtime_player_target_action_direction(std::span<u8> mind,
    const RuntimePlayerAttachmentEligibility& body, const Actor& actor,
    const RuntimeMindTarget& target, std::span<const u8> target_body,
    const Actor* focus); // Xbox 0x0007DDA0
struct RuntimePlayerMindTargetControl
{
    RuntimePlayerMindFocus& focus;
    RuntimePlayerMindProposal& proposal;
    RuntimePlayerMindFeedback& feedback;
    const RuntimeActorClock& clock;
    std::span<const u8> address_zero;
};
void update_runtime_player_target_control(RuntimePlayerMindTargetControl& context,
    u32 enabled, float heading); // Xbox 0x0007E4B0
}
