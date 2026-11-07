#pragma once
#include "mind_fields.h"
#include "input_tracking_record.h"
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
GBTG_MIND_VIEW(input_tracking_record, InputTrackingRecord)
static_assert(sizeof(InputTrackingRecord) == sizeof(InputTrackingRecord));

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
struct StickTrackingRecord
{
    u32 pressed = 0; // +0
    u32 state = 0; // +0x4, 1 released, 2 held
    float press_timer = 0.f; // +0x8, 0.2 at a press
    float unk_0C = 0.f; // +0xC, cleared at a press, decays
    float direction = 0.f; // +0x10
    float magnitude = 0.f; // +0x14, the last tick's
    float gesture_time = 0.f; // +0x18
    u32 gesture = 0; // +0x1C, 0 idle, 1 pushing, 2 easing off, 3 held
};
static_assert(sizeof(StickTrackingRecord) == 0x20);
GBTG_MIND_VIEW(stick_record, StickTrackingRecord)

// The player mind's own part after the base mind (0x7C bytes): the target its requests went to, and the stick.
struct PlayerMindRecord
{
    MindRecord base{}; // +0
    u32 unk_7C = 0; // +0x7C, a message's actor; the feedback tests it
    u32 unk_80 = 0; // +0x80, parameters +0xA0
    u32 unk_84 = 0; // +0x84, a source by address; messages naming it clear it (0x00140A40)
    u32 target = 0; // +0x88, the actor the last target request reached, by address
    float target_timer = 0.f; // +0x8C, clears target_count when it runs out
    u8 target_count = 0; // +0x90, the requests that reached the target
    std::array<u8, 3> unk_91{}; // +0x91, padding
    StickTrackingRecord stick{}; // +0x94
    u32 attachment_noticed = 0; // +0xB4, the attachment notice was raised for this target
    MindTargetRecord own_target{}; // +0xB8, the record a player mind targets itself through
    float unk_120 = 0.f; // +0x120
    u32 unk_124 = 0; // +0x124
    float unk_128 = 0.f; // +0x128
    float unk_12C = 0.f; // +0x12C
    float unk_130 = 0.f; // +0x130
    u32 unk_134 = 0; // +0x134
    Vec3 unk_138{}; // +0x138
    u32 unk_144 = 0; // +0x144
    float unk_148 = 0.f; // +0x148
    float unk_14C = 0.f; // +0x14C
    u32 unk_150 = 0; // +0x150
    u32 unk_154 = 0; // +0x154
    std::array<u8, 4> unk_158{}; // +0x158, never read or written by translated code
    float control_lock = 0.f; // +0x15C, positive: the player can't act on targets
    std::array<u8, 0x14> unk_160{}; // +0x160, never read or written by translated code
    u32 unk_174 = 0; // +0x174
    std::array<u8, 0x14> unk_178{}; // +0x178, never read or written by translated code
    u32 unk_18C = 0; // +0x18C
    float unk_190 = 0.f; // +0x190
    u32 unk_194 = 0; // +0x194
};
static_assert(sizeof(PlayerMindRecord) == 0x198);

GBTG_MIND_VIEW(player_mind_record, PlayerMindRecord)

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
