#pragma once

#include "body_state_actions.h"
#include "body_walk_alert_tick.h"
#include "body_state_fields.h"

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
// One walk row (0x10 bytes): the animation's name by address, then the rates at the band's two limits.
struct WalkTargetRow
{
    u32 name = 0; // +0, the animation's name, a retail address
    u32 found = 0; // +4, set by 0x00091820 when the name has a record (kept natively)
    float lower_rate = 0.f; // +8, the stride time at the band's lower speed
    float upper_rate = 0.f; // +0xC, and at its upper speed
};
static_assert(sizeof(WalkTargetRow) == 0x10);
// The rows block (0x4C bytes), copied from the parameters' +0xC. 0x000918C0, 0x00091820 and 0x00091A00 take it by
// address.
struct WalkTargetRows
{
    // +0: row direction + band * 2: band 0 (slow) and 1 (fast), each with two directions (0x000A61B0).
    std::array<WalkTargetRow, 4> rows{};
    // +0x40: the bands' speed limits as fractions of the body's step speed: band b runs from limits[b] to
    // limits[b + 1].
    std::array<float, 3> band_limits{};
};
static_assert(sizeof(WalkTargetRows) == 0x4C);
// The walk-target parameters (0x60 bytes, 0x00091A90).
namespace walk_target_parameter
{
constexpr RetailRecord<0x0C, sizeof(WalkTargetRows)> rows{};
constexpr RetailField<u32, 0x58> settle_turn{};
constexpr RetailField<u16, 0x5C> arrived_key{};
constexpr RetailField<u16, 0x5E> stopped_key{};
constexpr u32 kSize = 0x60;
static_assert(retail_fits(kSize, stopped_key));
} // namespace walk_target_parameter
// The walk-target state's own fields at their retail offsets (0x7C bytes), after the body-state base
// (BodyStateBase).
struct WalkTargetState
{
    BodyStateBase base{}; // +0
    WalkTargetRows rows{}; // +0x18
    // +0x64: 1 s at construction (DAT_0032B0F8), ticking down: the walk cannot arrive until it is 0 (0x000A6640).
    float arrival_delay = 0.f;
    u32 arrived = 0; // +0x68, set by 0x000A6640 once the body faces the target; next_key holds then
    u32 settle_turn = 0; // +0x6C, set: 0x000A67C0 settles the body's turn (0x00134720)
    u16 arrived_key = 0; // +0x70, next_key once arrived
    u16 stopped_key = 0; // +0x72, next_key once the body has stopped
    u32 direction = 0; // +0x74, the playing row's direction; -1 (none) at construction
    u32 band = 0; // +0x78, the playing row's band
};
static_assert(sizeof(WalkTargetState) == 0x7C);
void walk_target_object_sizes(ObjectSizes& output); // Xbox 0x00091A90
void find_runtime_walk_target_records(WalkTargetRows& rows,
    RuntimeBodyStateContext& context); // Xbox 0x00091820
bool initialize_runtime_walk_target(WalkTargetState& state, std::span<const u8> parameters,
    RuntimeBodyStateContext& context); // Xbox 0x000A6380
void destroy_runtime_walk_target(WalkTargetState& state, RuntimeBodyStateContext& context); // Xbox 0x000A64D0
// Row `index`'s record; the native word at row +4 only says whether one was found.
const AnimationRecord* runtime_walk_target_record(const u8* state_key, u32 index);

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
i32 runtime_walk_target_band(const WalkTargetRows& rows, RuntimeWalkTargetTick& context); // Xbox 0x00091A00
// 0 or 1: which half-circle the turn from the actor's yaw to body +0x33C lies in.
u32 runtime_walk_target_side(float turn); // Xbox 0x000A6140
u32 runtime_walk_target_direction(RuntimeWalkTargetTick& context); // Xbox 0x000A61B0, EAX body
void aim_runtime_walk_target(const WalkTargetState& state, RuntimeWalkTargetTick& context); // Xbox 0x000A67C0
// The actor's distance to body +0x3C, returned in x87.
long double runtime_walk_target_distance(RuntimeWalkTargetTick& context); // Xbox 0x000A6950, EAX body
void select_runtime_walk_target_row(WalkTargetState& state,
    RuntimeWalkTargetTick& context); // Xbox 0x000A6280, EAX body, EDI state
// The row's playback rate at `speed` between the band's limits (ECX body, EAX direction, EDI band, EBX rows).
float runtime_walk_target_rate(const WalkTargetRows& rows, u32 direction, u32 band, float speed,
    RuntimeWalkTargetTick& context); // Xbox 0x000918C0
void retime_runtime_walk_target(const WalkTargetState& state,
    RuntimeWalkTargetTick& context); // Xbox 0x000A62D0, ESI body
void settle_runtime_walk_target(WalkTargetState& state,
    RuntimeWalkTargetTick& context); // Xbox 0x000A6640, EAX body, EDI state
void tick_runtime_walk_target(WalkTargetState& state, RuntimeWalkTargetTick& context); // Xbox 0x000A6520
// Vtable +0x1C: the next key (+0xC), with actor +0x84 set when the walk has arrived (+0x68).
u16 runtime_walk_target_next_key(const WalkTargetState& state, ActorAnimationState& animation); // Xbox 0x00091F70

// Type 0x6E, entityStateFlyAlert (getter 0x00091AB0, vtable 0x005058A0): the walk-target rows flown rather than
// walked. It keeps the walk target's sizes (0x00091A90), rows (+0x18) and next key (0x00091F70); the stick steers it
// level with the third input pair lifting it, and it plays the parameters' hover animation (+0x58) from the start.
namespace fly_alert_parameter
{
constexpr RetailRecord<0x0C, sizeof(WalkTargetRows)> rows{};
constexpr RetailField<u32, 0x58> hover{}; // the hover animation's name, a retail address
constexpr RetailField<u16, 0x5C> arrived_key{};
constexpr RetailField<u16, 0x5E> stopped_key{};
constexpr u32 kSize = 0x60;
static_assert(retail_fits(kSize, stopped_key));
} // namespace fly_alert_parameter
struct FlyAlertState
{
    BodyStateBase base{}; // +0
    WalkTargetRows rows{}; // +0x18
    float arrival_delay = 0.f; // +0x64, 1 s at construction, ticking down
    u32 arrived = 0; // +0x68, set once the body faces its heading; read by 0x00091F70
    u16 arrived_key = 0; // +0x6C
    u16 stopped_key = 0; // +0x6E
    u32 direction = 0; // +0x70, -1 (none) at construction
    u32 band = 0; // +0x74
    u32 hover = 0; // +0x78, set when the hover animation's record was found
};
static_assert(sizeof(FlyAlertState) == 0x7C);
// Getter 0x00091AB0 keeps 0x00091F70 at +0x1C, and 0x00091820 finds the rows' records, for both states alike.
static_assert(offsetof(FlyAlertState, rows) == offsetof(WalkTargetState, rows)
    && offsetof(FlyAlertState, arrived) == offsetof(WalkTargetState, arrived));
bool initialize_runtime_fly_alert(FlyAlertState& state, std::span<const u8> parameters,
    RuntimeBodyStateContext& context); // Xbox 0x00091C10
void destroy_runtime_fly_alert(const FlyAlertState& state, RuntimeBodyStateContext& context); // Xbox 0x00091D90
// 0 or 1: which half-circle the turn lies in, over DAT_00428674's bounds.
u32 runtime_fly_alert_side(float turn); // Xbox 0x000917B0
u32 runtime_fly_alert_direction(RuntimeWalkTargetTick& context); // Xbox 0x00091990, EAX body
// The stick's level flight and the third pair's lift (EAX body, EBX input).
void steer_runtime_fly_alert(RuntimeWalkTargetTick& context); // Xbox 0x000923B0
// With an alert mind target the body faces it (body +0x3C, +0x730).
void aim_runtime_fly_alert(RuntimeWalkTargetTick& context); // Xbox 0x00092280, EBX body
void select_runtime_fly_alert_row(FlyAlertState& state,
    RuntimeWalkTargetTick& context); // Xbox 0x00091B10, EAX body, EDI state
void retime_runtime_fly_alert(const FlyAlertState& state, RuntimeWalkTargetTick& context); // Xbox 0x00091B60, ESI body
void settle_runtime_fly_alert(FlyAlertState& state,
    RuntimeWalkTargetTick& context); // Xbox 0x00091FA0, EAX body, EDI state
// `ground` is body +0x15C byte 0: bit 0x40 holds the aim.
void tick_runtime_fly_alert(FlyAlertState& state, RuntimeWalkTargetTick& context, u8 ground); // Xbox 0x00091E10
}
