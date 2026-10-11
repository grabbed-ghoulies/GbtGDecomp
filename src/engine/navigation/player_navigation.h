#pragma once
#include "floor_tracking.h"
#include "../entities/actor_clock.h"
#include "../frontend/dialogue_input.h"

namespace gbtg
{
struct RuntimePlayerNavigation
{
    entity_detail::NavigationPathSearch*& search; // Mind +0x1C.
    Actor& actor;
    std::span<const u8> body;
    i32& floor; // Actor +0xCEC.
    entity_detail::NavigationCellRegistration*& registration; // Actor +0xCE8.
    const RuntimeActorClock& clock;
};
u32 request_runtime_player_navigation_path(RuntimePlayerNavigation& context,
    const Vec3& target, i32 target_floor, Vec3& direction,
    u32& changed_floor); // Xbox 0x00136EB0

struct RuntimePlayerNavigationAttachment
{
    u8 state; // Mind +0x10 -> +0x14.
    entity_detail::NavigationCellRegistration*& registration; // Attached actor +0xCE8.
    i32& floor; // Attached actor +0xCEC.
};
struct RuntimePlayerNavigationControl
{
    RuntimePlayerNavigation& navigation;
    Actor& body_actor; // Body +8.
    EntityBitBlock& flags; // Body +0x15C.
    const RuntimeDialogueInputDevice& input; // Actor +0x960.
    const RuntimeDialogueInputState& input_state;
    RuntimeInputAxisWriteDevice& writable;
    std::span<u8> keyboard;
    RuntimePlayerNavigationAttachment* attachment; // Mind +0x10.
};
bool runtime_player_motion_allows_navigation(RuntimePlayerNavigationControl& context,
    u32& forced, float& alignment); // Xbox 0x00137500
bool runtime_player_input_aligns_with_motion(RuntimePlayerNavigationControl& context,
    float& alignment); // Xbox 0x00137610
bool runtime_player_navigation_motion(RuntimePlayerNavigationControl& context,
    u32& forced, float& motion_alignment, float& input_alignment); // Xbox 0x00137460
bool runtime_player_navigation_target_is_near(RuntimePlayerNavigationControl& context,
    const Vec3& target); // Xbox 0x00137790
bool runtime_player_navigation_fallback(RuntimePlayerNavigationControl& context,
    const Vec3& target); // Xbox 0x001373F0
entity_detail::NavigationCellRegistration* suspend_runtime_navigation_attachment(
    RuntimePlayerNavigationAttachment* attachment, i32& floor); // Xbox 0x00137100
u32 drive_runtime_player_navigation(RuntimePlayerNavigationControl& context,
    const Vec3& target, i32 target_floor, float radius, float approach,
    float speed, u32 keep_attachment, u32& failed); // Xbox 0x00136B00
u32 drive_runtime_mind_vertical_navigation(RuntimePlayerNavigationControl& context,
    const Vec3& target, i32 target_floor, float height, float tolerance,
    float radius, float approach, float speed, float vertical_speed,
    u32 keep_attachment, u32& failed); // Xbox 0x00136D60
u32 handle_runtime_player_navigation_request(RuntimePlayerNavigationControl& context,
    std::span<const u8> message); // Xbox 0x00136620
}
