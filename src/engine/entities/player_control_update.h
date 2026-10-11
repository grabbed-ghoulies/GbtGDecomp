#pragma once
#include "body_request_selection.h"
#include "../navigation/player_avoidance.h"
#include "player_mind_control.h"

namespace gbtg
{
struct RuntimePlayerControlUpdate
{
    RuntimePlayerMindActionControl& actions;
    RuntimePlayerMindPendingAttachment& pending;
    RuntimePlayerMindGaze& gaze;
    RuntimePlayerMindInactivity& inactivity;
    RuntimePlayerMindControl& control;
    RuntimeMindBoost& boost;
    RuntimePlayerNavigationAvoidance& avoidance;
    const ActorAnimationState& animation;
    const std::span<const u8>& boost_preset; // DAT_002FFF24.
    const Vec3& camera_direction;
    const u32& invert; // DAT_005122D8.
    u32& random;
};
void update_runtime_player_control(RuntimePlayerControlUpdate& context); // Xbox 0x0007D130
}
