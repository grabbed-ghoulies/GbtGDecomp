#pragma once
#include "body_state_actions.h"

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
// The first speed band whose limit exceeds `speed`, or the last band (EAX state, EDX body, XMM5 speed). The walk alert
// and the fly state both come here, with their rows at the same offsets.
i32 runtime_walk_alert_band(i32 count, std::span<const WalkAlertRow> rows, std::span<const u8> body, const Actor& actor,
    float speed, const RuntimeBodyDamageTables& tables, std::span<const float> mode_scales); // Xbox 0x000A5FD0
void retime_runtime_walk_alert(const WalkAlertState& state, float speed,
    RuntimeWalkAlertTick& context); // Xbox 0x000A5C70
void select_runtime_walk_alert_band(WalkAlertState& state, RuntimeWalkAlertTick& context); // Xbox 0x000A5DC0
void steer_runtime_walk_alert(const WalkAlertState& state, RuntimeWalkAlertTick& context); // Xbox 0x000A5A60
void select_runtime_walk_alert_key(WalkAlertState& state, RuntimeWalkAlertTick& context); // Xbox 0x000A5EA0
void tick_runtime_walk_alert(WalkAlertState& state, RuntimeWalkAlertTick& context); // Xbox 0x000A59C0
// The fly state (type 0x6D) over the walk alert's views; `orient` is unused.
// The stick sets the horizontal velocity and the third pair's second axis the vertical one (EAX body, EBX input).
void steer_runtime_fly_state(const FlyState& state, RuntimeWalkAlertTick& context); // Xbox 0x00091210
void retime_runtime_fly_state(const FlyState& state, float speed,
    RuntimeWalkAlertTick& context); // Xbox 0x00091480, EAX body, EBX state
// The band changes once the actor's speed and the steered speed agree on it (EAX body, ECX state).
void select_runtime_fly_band(FlyState& state, RuntimeWalkAlertTick& context); // Xbox 0x000915D0
void select_runtime_fly_key(FlyState& state, RuntimeWalkAlertTick& context); // Xbox 0x000916C0, ESI state
float runtime_body_scaled_step_speed(std::span<const u8> body, const Actor& actor,
    const RuntimeBodyDamageTables& tables, std::span<const float> mode_scales); // Xbox 0x00025870, EAX body
void tick_runtime_fly_state(FlyState& state, RuntimeWalkAlertTick& context); // Xbox 0x00091190
// Message 0x53 calls 0x00023890 with ECX 1 and the state's body.
void message_runtime_walk_alert(std::span<const u8> message,
    const std::function<void()>& stop); // Xbox 0x000A5A40
struct RuntimeBodyActionMessages;
// Message 0x53's answer when its +4 word matches `key`: +8 is 0 for a body with no energy (+0x94); otherwise the
// body takes action 0x117 (for a nonzero word) or its own action key (vtable +0x20) through 0x00024B20, and +8 is 1.
void stop_runtime_walk_alert(std::span<u8> body, std::span<u8> message, u32 key,
    const std::function<u16()>& action_key, RuntimeBodyActionMessages& actions); // Xbox 0x00023890, EAX body, ECX key
}
