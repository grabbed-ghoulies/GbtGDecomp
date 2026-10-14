#pragma once

#include "body_action_selection.h"
#include "body_damage_tables.h"
#include "actor_clock.h"
#include "../frontend/dialogue_input.h"

namespace gbtg
{
// Native views of a walk-alert state's body (state +8) and actor (body +8).
struct RuntimeWalkAlertTick
{
    std::span<u8> body;
    Actor& actor;
    const EntityBitBlock& flags; // Body +0x15C -> +8.
    RuntimeBodyActionSelection& actions;
    const RuntimeActorClock& clock;
    const RuntimeDialogueInputDevice* input; // The tick's input word (ECX of 0x000A5A60), or null.
    const RuntimeDialogueInputState& input_state;
    const RuntimeBodyDamageTables& tables;
    std::span<const float> mode_scales; // DAT_00217EC0.
    bool mind_present; // Actor +0x958.
    u8 mind_alert; // Actor +0x958 -> +0x20.
    std::function<void()> orient; // 0x00134720, ESI body.
};
// Body +0x328 rate scaled by the body's surface mode (EDX body).
float runtime_body_turn_rate(std::span<const u8> body, const Actor& actor, const RuntimeBodyDamageTables& tables,
    std::span<const float> mode_scales); // Xbox 0x000258D0
// The turn-to-stride ratio; a nonzero `fixed` keeps only the surface mode (EAX body).
float runtime_body_turn_ratio(std::span<const u8> body, const Actor& actor, u32 fixed,
    const RuntimeBodyDamageTables& tables, std::span<const float> mode_scales); // Xbox 0x00133D50
// The slot-0 animation's stride time at `speed`, clamped to [0.1, 50]; false when there is no stride
// (EAX body, EDI output).
bool runtime_body_stride_duration(float speed, const Actor& actor, const ActorAnimationState& animation,
    float& duration); // Xbox 0x00133C90
// The first speed band whose limit exceeds `speed`, or the last band (EAX state, EDX body, XMM5 speed).
i32 runtime_walk_alert_band(std::span<const u8> state, std::span<const u8> body, const Actor& actor,
    float speed, const RuntimeBodyDamageTables& tables, std::span<const float> mode_scales); // Xbox 0x000A5FD0
void retime_runtime_walk_alert(std::span<const u8> state, float speed,
    RuntimeWalkAlertTick& context); // Xbox 0x000A5C70
void select_runtime_walk_alert_band(std::span<u8> state, RuntimeWalkAlertTick& context); // Xbox 0x000A5DC0
void steer_runtime_walk_alert(std::span<const u8> state, RuntimeWalkAlertTick& context); // Xbox 0x000A5A60
void select_runtime_walk_alert_key(std::span<u8> state, RuntimeWalkAlertTick& context); // Xbox 0x000A5EA0
void tick_runtime_walk_alert(std::span<u8> state, RuntimeWalkAlertTick& context); // Xbox 0x000A59C0
// Message 0x53 calls 0x00023890 with ECX 1 and the state's body.
void message_runtime_walk_alert(std::span<const u8> message,
    const std::function<void()>& stop); // Xbox 0x000A5A40
}
