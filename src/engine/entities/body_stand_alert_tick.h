#pragma once
#include "body_state_actions.h"

#include "body_action_selection.h"
#include "actor_clock.h"
#include "../frontend/dialogue_input.h"

namespace gbtg
{
struct RuntimeStandAlertTick
{
    std::span<u8> body;
    Actor& actor;
    const EntityBitBlock& flags;
    const ActorAnimationState& animation;
    RuntimeBodyActionSelection& actions;
    const RuntimeActorClock& clock;
    const RuntimeDialogueInputDevice& input;
    const RuntimeDialogueInputState& input_state;
    bool mind_present;
    u8 mind_alert;
    float input_threshold; // DAT_0032B22C.
    std::function<u16()> moving_action; // Body +0x40.
    std::function<void()> orient; // 0x00134720, ESI body.
};
void repeat_runtime_stand_alert_animation(RuntimeStandAlertTick& context); // Xbox 0x000A2A90
void tick_runtime_stand_alert(StandAlertState& state, RuntimeStandAlertTick& context); // Xbox 0x000A2930
}
