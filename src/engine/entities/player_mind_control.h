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
// The struggle states' and the boost's input tracking (0x18 bytes, 0x00109810): how hard the stick is being worked.
namespace input_tracking_field
{
constexpr RetailField<std::array<float, 2>, 0x00> axes{}; // the last tick's
constexpr RetailField<float, 0x08> activity{}; // rises while the stick moves, up to 1
constexpr RetailField<float, 0x0C> accumulation{}; // rises up to the preset's cap
constexpr RetailField<float, 0x10> hold_timer{}; // the state flips once it runs out
constexpr RetailField<u32, 0x14> state{}; // 1 waits for full activity, 2 for none
constexpr u32 kSize = 0x18;
} // namespace input_tracking_field

// The tracking's preset (0x10 bytes).
namespace input_tracking_preset_field
{
constexpr RetailField<float, 0x0> threshold{}; // the movement that counts as working the stick
constexpr RetailField<float, 0x4> rise{};
constexpr RetailField<float, 0x8> decay{};
constexpr RetailField<float, 0xC> cap{}; // the accumulation's
} // namespace input_tracking_preset_field

// The player mind's stick tracking (+0x94, 0x20 bytes; 0x0007D460, 0x0007D5A0): presses, the direction and a
// push-and-release gesture.
namespace stick_tracking_field
{
constexpr RetailField<u32, 0x00> pressed{};
constexpr RetailField<u32, 0x04> state{}; // 1 released, 2 held
constexpr RetailField<float, 0x08> press_timer{}; // 0.2 at a press
constexpr RetailField<float, 0x0C> unk_0C{}; // cleared at a press, decays
constexpr RetailField<float, 0x10> direction{};
constexpr RetailField<float, 0x14> magnitude{}; // the last tick's
constexpr RetailField<float, 0x18> gesture_time{};
constexpr RetailField<u32, 0x1C> gesture{}; // 0 idle, 1 pushing, 2 easing off, 3 held
constexpr u32 kSize = 0x20;
} // namespace stick_tracking_field

// The player mind's own part after the base mind (0x7C bytes): the target its requests went to, and the stick.
namespace player_mind_field
{
constexpr RetailField<u32, 0x88> target{}; // the actor the last target request reached, by address
constexpr RetailField<float, 0x8C> target_timer{}; // clears target_count when it runs out
constexpr RetailField<u8, 0x90> target_count{}; // the requests that reached the target
constexpr RetailRecord<0x94, stick_tracking_field::kSize> stick{}; // stick_tracking_field
constexpr RetailField<u32, 0xB4> attachment_noticed{}; // the attachment notice was raised for this target
constexpr RetailField<float, 0x15C> control_lock{}; // positive: the player can't act on targets
} // namespace player_mind_field

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
