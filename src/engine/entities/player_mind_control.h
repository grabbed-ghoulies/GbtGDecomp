#pragma once
#include "player_mind_requests.h"
#include "../frontend/dialogue_input.h"
#include "actor_model_anchors.h"
#include "body_gaze.h"

namespace gbtg
{
struct RuntimePlayerMindShapeTarget
{
    std::span<u8> mind;
    std::span<const u8> body;
    Actor& actor;
    ActorLiveAttachment* attachment; // Body actor +0x8F8.
    const EntityBitBlock& flags;
    const i32& floor; // Body actor +0xCEC.
    u32 mind_address;
};
std::span<u8> refresh_runtime_player_shape_target(RuntimePlayerMindShapeTarget& context,
    float angle, float distance); // Xbox 0x0007EA20
std::span<const u8> redirect_runtime_player_shape_target(RuntimePlayerMindShapeTarget& context,
    const RuntimePlayerAttachmentEligibility& body, std::span<const u8> candidate,
    float angle); // Xbox 0x0007E9E0
bool runtime_player_mind_accepts_target(const RuntimePlayerAttachmentEligibility& body,
    std::span<const u8> target_body, const Actor& target_actor,
    std::span<const u8> candidate, const LiveScript& events,
    std::span<const u8> address_zero); // Xbox 0x0007E960
void set_runtime_mind_gaze_anchor(std::span<u8> mind, u32 anchor_address,
    u32& random_state); // Xbox 0x0002A130
struct RuntimePlayerMindGaze
{
    std::span<u8> mind;
    std::span<u8> body;
    Actor& actor;
    Actor& body_actor;
    Actor*& target; // Mind +0xC.
    ActorModelAnchor*& anchor; // Mind +0x30.
    const RuntimeDialogueInputDevice& input;
    const RuntimeDialogueInputState& input_state;
    const std::unordered_map<ActorModelAnchor*, u32>& addresses;
    u32& random_state;
};
void update_runtime_player_mind_gaze(RuntimePlayerMindGaze& context,
    i32 check_path, float maximum_height, float radius); // Xbox 0x00137140
bool request_runtime_player_idle_action(std::span<u8> mind,
    RuntimeBodyAttachmentRequests& body, u32& random_state); // Xbox 0x0007E3A0
void randomize_runtime_player_request_value(std::span<const float, 2> range,
    float& output, u32& random_state); // Xbox 0x0007EDF0
u32 prepare_runtime_player_mind_request(std::span<u8> mind, Actor& actor,
    const Vec3* reference_position, std::span<u8> message,
    u32& random_state); // Xbox 0x0007EBC0
struct RuntimePlayerMindControl
{
    std::span<u8> mind;
    RuntimePlayerAttachment& attachment;
    EntityBitBlock& flags;
    const RuntimeActorClock& clock;
    const RuntimeDialogueInputDevice& input;
    const RuntimeDialogueInputState& input_state;
    std::function<u16()> primary_action_key; // Actual body vtable +0x20.
};
struct RuntimePlayerMindInactivity
{
    std::span<u8> mind;
    EntityBitBlock& flags;
    const RuntimeActorClock& clock;
    const RuntimeDialogueInputDevice& input;
    const RuntimeDialogueInputState& input_state;
};
void update_runtime_player_mind_inactivity(RuntimePlayerMindInactivity& control,
    RuntimeBodyAttachmentRequests& body, const ActorAnimationState& animation,
    u32& random_state); // Xbox 0x0007E160
struct RuntimePlayerMindPendingAttachment
{
    std::span<u8> mind;
    RuntimeBodyAttachedActor*& pending; // Mind +0x134.
    RuntimeBodyAttachedActor*& action; // Mind +0x18.
    const RuntimePlayerAttachmentEligibility& eligibility;
    std::span<u8> body;
    RuntimeBodyActionSelection& actions;
    const RuntimeDialogueInputDevice& input;
    const RuntimeDialogueInputState& input_state;
    const std::unordered_map<RuntimeBodyAttachedActor*, std::span<const u8>>& links;
    std::function<u32(RuntimeBodyAttachedActor&)> attach; // Actual body vtable +0x5C.
};
void update_runtime_player_pending_attachment(RuntimePlayerMindPendingAttachment& context); // Xbox 0x0007D210
void update_runtime_player_mind_hold_timer(RuntimePlayerMindControl& context); // Xbox 0x0007D280
void release_runtime_player_mind_input_attachment(RuntimePlayerMindControl& context); // Xbox 0x0007D300
void rotate_runtime_player_input_axes(std::array<float, 2>& axes,
    const Vec3& camera_direction); // Xbox 0x00140AE0
void transform_runtime_player_mind_input(std::span<const u8> body, u32 invert,
    const RuntimeDialogueInputDevice& input, const RuntimeDialogueInputState& state,
    RuntimeInputAxisWriteDevice& writable, std::span<u8> keyboard,
    const Vec3& camera_direction); // Xbox 0x0007EE40
void update_runtime_player_input_tracking(std::span<u8> tracking,
    const std::array<float, 2>& axes, std::span<const u8> preset,
    u32 accumulate, const RuntimeActorClock& clock); // Xbox 0x00109810
bool update_runtime_player_boost_timer(RuntimeMindBoost& boost,
    RuntimePlayerMindControl& control, const std::span<const u8>* preset,
    float multiplier); // Xbox 0x00137840
void update_runtime_player_input_gesture(std::span<u8> tracking,
    float magnitude, float step); // Xbox 0x0007D5A0
void update_runtime_player_input_direction(std::span<u8> tracking,
    const std::array<float, 2>& axes, float step, u32 special); // Xbox 0x0007D460
}
