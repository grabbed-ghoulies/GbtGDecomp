#pragma once

#include "body_state_actions.h"
#include "body_walk_alert_tick.h"

namespace gbtg
{
// Angle helpers over [0, 2pi) arcs.
// True when `angle` lies in the arc from `start` to `end`; an `end` of exactly 2pi is not wrapped.
bool runtime_angle_in_arc(float angle, float start, float end); // Xbox 0x001009A0
// Whichever of `lower` and `upper` is the shorter signed turn from `angle`.
float runtime_nearest_arc_bound(float angle, float lower, float upper); // Xbox 0x00100B80
// `angle` wrapped into [0, 2pi), or the nearest bound when it lies outside the arc from `lower` to `upper`.
float runtime_clamp_angle_to_arc(float angle, float lower, float upper); // Xbox 0x00100A80
// `target` + the turn from `target` to `heading` clamped to [-limit, limit] (XMM0 heading).
float runtime_turn_within_arc(float heading, float target, float limit); // Xbox 0x00092120
// The [0, 1] facing weight of `heading` against `target` (XMM0 heading).
float runtime_facing_weight(float heading, float target); // Xbox 0x000921E0

// Type 0x8F (getter 0x000A6220, vtable 0x00505CC0): the walk a woken skeleton takes toward its mind's target.
// State +0x18 holds four {name, record, rate, rate} rows (2 speed bands x 2 directions) and the band limits at
// +0x58..+0x60.
void walk_target_object_sizes(ObjectSizes& output); // Xbox 0x00091A90
void find_runtime_walk_target_records(std::span<u8> rows,
    RuntimeBodyStateContext& context); // Xbox 0x00091820
bool initialize_runtime_walk_target(std::span<u8> state, std::span<const u8> parameters,
    RuntimeBodyStateContext& context); // Xbox 0x000A6380
void destroy_runtime_walk_target(std::span<u8> state, RuntimeBodyStateContext& context); // Xbox 0x000A64D0
// Row `index`'s record; the native word at row +4 only says whether one was found.
const AnimationRecord* runtime_walk_target_record(std::span<const u8> state, u32 index);

// Native views of a walk-target state's body (state +8) and actor (body +8).
struct RuntimeWalkTargetTick
{
    std::span<u8> body;
    Actor& actor;
    RuntimeBodyActionSelection& actions;
    const RuntimeActorClock& clock;
    const RuntimeDialogueInputDevice* input; // Actor +0x960's input device, or null.
    const RuntimeDialogueInputState& input_state;
    const RuntimeBodyDamageTables& tables;
    std::span<const float> mode_scales; // DAT_00217EC0.
    std::span<const u8> mind; // Actor +0x958, or empty.
    std::function<Vec3(u32)> actor_position; // 0x000412A0 of the actor at an address.
};
// The stick's walk velocity (EAX body).
void steer_runtime_body_by_stick(RuntimeWalkTargetTick& context); // Xbox 0x00019840
// Speed band 0 or 1 from rows +0x40..+0x48 (EAX body).
i32 runtime_walk_target_band(std::span<const u8> rows, RuntimeWalkTargetTick& context); // Xbox 0x00091A00
// 0 or 1: which half-circle the turn from the actor's yaw to body +0x33C lies in.
u32 runtime_walk_target_side(float turn); // Xbox 0x000A6140
u32 runtime_walk_target_direction(RuntimeWalkTargetTick& context); // Xbox 0x000A61B0, EAX body
void aim_runtime_walk_target(std::span<const u8> state, RuntimeWalkTargetTick& context); // Xbox 0x000A67C0
// The actor's distance to body +0x3C, returned in x87.
long double runtime_walk_target_distance(RuntimeWalkTargetTick& context); // Xbox 0x000A6950, EAX body
void select_runtime_walk_target_row(std::span<u8> state,
    RuntimeWalkTargetTick& context); // Xbox 0x000A6280, EAX body, EDI state
// The row's playback rate at `speed` between the band's limits (ECX body, EAX direction, EDI band, EBX rows).
float runtime_walk_target_rate(std::span<const u8> rows, u32 direction, u32 band, float speed,
    RuntimeWalkTargetTick& context); // Xbox 0x000918C0
void retime_runtime_walk_target(std::span<const u8> state,
    RuntimeWalkTargetTick& context); // Xbox 0x000A62D0, ESI body
void settle_runtime_walk_target(std::span<u8> state,
    RuntimeWalkTargetTick& context); // Xbox 0x000A6640, EAX body, EDI state
void tick_runtime_walk_target(std::span<u8> state, RuntimeWalkTargetTick& context); // Xbox 0x000A6520
// Vtable +0x1C: the next key (+0xC), with actor +0x84 set when the walk has arrived (+0x68).
u16 runtime_walk_target_next_key(std::span<const u8> state, ActorAnimationState& animation); // Xbox 0x00091F70
}
