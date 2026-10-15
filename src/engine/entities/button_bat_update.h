#pragma once
#include "actor_frame_update.h"

namespace gbtg
{
void update_button_bat(ButtonBatScreen& bat, ActorFrameState& state,
    ActorFrameContext& context); // Xbox 0x0006D5C0
// Starts the bat's flight from where it is to a point (EBX), offset by the scale: "flybetween" over its flight
// time, kept at 0.2 when it was already flying.
void fly_runtime_button_bat(ButtonBatScreen& bat, ActorAnimationState& animation, const ButtonBatPoint& target,
    float scale, AnimationResourceRegistry& registry, ResourceErrorState& error,
    u32& random_state); // Xbox 0x0006DA00, ESI the bat
}
