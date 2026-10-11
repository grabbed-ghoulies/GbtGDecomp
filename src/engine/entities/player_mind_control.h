#pragma once
#include "player_mind_requests.h"
#include "../frontend/dialogue_input.h"

namespace gbtg
{
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
bool update_runtime_player_boost_timer(RuntimePlayerMindBoost& boost,
    RuntimePlayerMindControl& control, const std::span<const u8>* preset,
    float multiplier); // Xbox 0x00137840
void update_runtime_player_input_gesture(std::span<u8> tracking,
    float magnitude, float step); // Xbox 0x0007D5A0
void update_runtime_player_input_direction(std::span<u8> tracking,
    const std::array<float, 2>& axes, float step, u32 special); // Xbox 0x0007D460
}
