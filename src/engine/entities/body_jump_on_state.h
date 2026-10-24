#pragma once

#include "actor_clock.h"
#include "body_action_selection.h"
#include "body_spatial_notifications.h"
#include "body_state_actions.h"
#include "body_state_fields.h"
#include "intro_notices.h"
#include "../frontend/dialogue_input.h"
#include "gbtg/anim.h"
#include "gbtg/object_factory.h"
#include "gbtg/sound_runtime.h"

#include <functional>

namespace gbtg
{
// Factory type 0x79, entityStateJumpOnOff (getter 0x0009A110, vtable 0x00505A00) over the body state (type 9): the
// imp's leap onto its target's head. Phase (+0x68) 1 waits in the take-off animation (+0x148) for the jump window
// (DAT_004286A0), claims the target's body (+0x718) and checks the way is clear; 2 leaps in an arc onto the target's
// head marker (model record kind 4, index DAT_00428684); 3 rides it (the ride record at +0x80: ride, hold and
// hit animations while the rider steers its host and the host's pad shakes it off); 4 leaps off to a landing
// point; 5 plays the landing; 6 ends with the body's next action. +0x14 / +0x18 are the empty 0x001628B0 and +0x1C
// is 0x0002B620. State +8 is the body.
namespace jump_on_state_parameter
{
constexpr RetailField<u32, 0x14> take_off{}; // animation names (XBE addresses)
constexpr RetailField<u32, 0x18> jump_off{};
constexpr RetailField<u32, 0x1C> ride{}; // ride phase 1
constexpr RetailField<u32, 0x20> hit{}; // ride phase 3
constexpr RetailField<u32, 0x24> hold{}; // ride phase 2
constexpr RetailField<u32, 0x28> keep_riding{}; // set: the ride ends only when shaken off, and then knocked down
constexpr RetailField<float, 0x2C> take_off_blend{}; // stored into DAT_004286B0
constexpr RetailField<std::array<char, 0x80>, 0x30> hold_sound{};
constexpr u32 kSize = 0xB0;
static_assert(retail_fits(kSize, hold_sound));
} // namespace jump_on_state_parameter
namespace jump_on_state_field
{
constexpr RetailField<Vec3, 0x18> leap_target_euler{}; // the leap record (0x00103650) starts here
constexpr RetailField<Vec3, 0x48> leap_end{};
constexpr RetailField<u32, 0x68> phase{};
constexpr RetailField<u32, 0x6C> shape_mode{}; // 0x0001FEB0's previous mode
constexpr RetailField<u32, 0x70> released{}; // clear while the body holds its latches (0x0009A420)
constexpr RetailField<u32, 0x74> knocked{}; // the ride ended by shaking: end in body slot 0x34's action
constexpr RetailField<u32, 0x78> keep_riding{}; // parameters +0x28
constexpr RetailField<u32, 0x7C> aborted{}; // messages 0x5D, 0x7C, 0x80 and 0x86 kind 7
constexpr u32 kRide = 0x80; // the ride record
constexpr RetailField<u32, 0x148> take_off{}; // set when the animation was found (the records are the context's)
constexpr RetailField<u32, 0x14C> jump_off{};
constexpr RetailField<u32, 0x150> claimed{}; // the target's body +0x718 names this actor
constexpr RetailField<u32, 0x154> target{}; // the claimed target's actor
constexpr u32 kSize = 0x158;
static_assert(retail_fits(kSize, target));
} // namespace jump_on_state_field
// The ride record (state +0x80).
namespace ride_field
{
constexpr RetailField<u32, 0x00> phase{}; // 1 riding, 2 holding on while shaken, 3 hitting the host
constexpr RetailField<u32, 0x04> strikes{};
constexpr RetailField<u32, 0x08> ride{}; // set when the animation was found (state +0x88)
constexpr RetailField<u32, 0x0C> hit{}; // state +0x8C
constexpr RetailField<u32, 0x10> hold{}; // state +0x90
constexpr RetailField<float, 0x14> wobble_delay{}; // 0x001050A0's timer, then its range
constexpr RetailField<float, 0x18> wobble_minimum{};
constexpr RetailField<float, 0x1C> wobble_maximum{};
constexpr RetailField<float, 0x20> wobble{}; // radians added to the host's heading
constexpr RetailField<float, 0x24> hold_time{}; // phase 2's least time
constexpr RetailField<float, 0x28> ride_time{}; // phase 1's time to the next hit
constexpr u32 kTracking = 0x2C; // 0x00109810's record: +0x34 shake, +0x38 its total, +0x40 its state
constexpr RetailField<float, 0x38> shaken{};
constexpr RetailField<u32, 0x40> shake_state{};
constexpr RetailField<std::array<char, 0x80>, 0x44> hold_sound{}; // state +0xC4
constexpr RetailField<u32, 0xC4> hat_intro{}; // the hat intro was queued (0x0013FE60)
constexpr u32 kSize = 0xC8;
static_assert(retail_fits(kSize, hat_intro));
} // namespace ride_field
// Another actor the state reads: its target, or the actor it rides.
struct RuntimeJumpOnPeer
{
    Actor* actor = nullptr;
    u32 address = 0;
    std::span<const u8> bytes; // The actor's instance bytes.
    std::span<u8> body; // Actor +0x95C; empty for an actor that is not a player.
    EntityBitBlock* bits = nullptr; // Body +0x15C.
    std::span<u8> mind; // Actor +0x958, or empty.
    const RuntimeBodyModelNodes* model = nullptr; // Actor +0x204's records (0x00040980).
    const RuntimeDialogueInputDevice* input = nullptr; // Actor +0x960, or null.
};
struct RuntimeJumpOnStateContext
{
    std::span<u8> body;
    Actor& actor; // Body +8.
    u32 actor_address;
    std::span<const u8> actor_bytes;
    EntityBitBlock& bits; // Body +0x15C.
    RuntimeBodyActionSelection& actions;
    const RuntimeActorClock& clock;
    ActorAnimationState& animation; // Actor +0x50.
    std::span<u8> mind; // Actor +0x958, or empty.
    entity_detail::ListHead& live_attachments; // Actor +0x8F4.
    entity_detail::LatchNode*& latches; // DAT_00511AA4.
    RuntimeBodySpatialNotifications& spatial; // 0x0001D780.
    RuntimeBodyStateContext shape; // 0x0001FEB0.
    RuntimeActorSoundQueues& sounds;
    std::span<u8> globals; // DAT_00428680, 0x60 bytes; the constructor writes +0x30 (DAT_004286B0).
    const Vec3& default_position; // DAT_0050DD80.
    const Vec3& default_euler; // DAT_0050DD74.
    u32& random; // DAT_0054DD7C.
    const u32& runtime_flags; // DAT_0050EA9C.
    std::span<const u32, 2> player_blocked; // DAT_0050E764.
    std::span<const u8> player_modes; // DAT_0030F144.
    const u32& player_mode; // DAT_0051248C.
    RuntimeIntroNotices& intro;
    u8& story_byte; // **(DAT_00506C6C + 8).
    const RuntimeDialogueInputState& input_state;
    float occupancy_initial;
    std::function<const char*(u32 address)> image_string; // The parameters' XBE names; null for 0.
    std::function<u16(u32 slot)> body_slot_key; // Body vtable +0x20 / +0x34.
    std::function<void(std::span<const u32>)> body_message; // Body vtable +0xC.
    std::function<void(u32 address, std::span<u32>)> message; // Any actor's vtable +0xC; replies land in place.
    std::function<void(u32 actor, u32 id)> actor_callouts; // 0x0003BE20 over an actor's +0x8C records, options 0.
    std::function<std::span<const u8>(u32 address)> target_at; // A mind target record (DAT_00545988).
    std::function<RuntimeJumpOnPeer(u32 actor_address)> peer_at; // Any scene actor; players fill the body views.
    std::function<u32(const void*)> address_of;
};
void jump_on_state_object_sizes(ObjectSizes& output); // Xbox 0x0009A0F0
bool initialize_runtime_jump_on_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimeJumpOnStateContext& context); // Xbox 0x0009A4C0
void destroy_runtime_jump_on_state(std::span<u8> state, RuntimeJumpOnStateContext& context); // Xbox 0x0009A710
void tick_runtime_jump_on_state(std::span<u8> state, RuntimeJumpOnStateContext& context); // Xbox 0x0009AB00
void message_runtime_jump_on_state(std::span<u8> state, std::span<const u32> message,
    RuntimeJumpOnStateContext& context); // Xbox 0x0009AE60
// The slot 0 animation passed `time` since its last step.
bool runtime_jump_on_window_open(float time, RuntimeJumpOnStateContext& context); // Xbox 0x0009A170, EAX actor
// The target's head marker exists and its body is up (+0x94) and not knocked away (bit 0x39).
bool runtime_jump_on_target_open(u32 target, RuntimeJumpOnStateContext& context); // Xbox 0x0009A1E0, EAX target
// The leap is off: aborted, unclaimed, paused (0x400), no target, or the way to it blocked.
bool runtime_jump_on_blocked(std::span<const u8> state, RuntimeJumpOnStateContext& context); // Xbox 0x0009A250
// The target's head marker position and euler.
bool runtime_jump_on_target_pose(Vec3& position, Vec3& euler,
    RuntimeJumpOnStateContext& context); // Xbox 0x0009A2E0, EAX actor, EBX euler
void runtime_jump_on_marker_position(const RuntimeJumpOnPeer& target, i32 marker,
    Vec3& output); // Xbox 0x0009A330, EAX target
void runtime_jump_on_marker_euler(const RuntimeJumpOnPeer& target, i32 marker,
    Vec3& output); // Xbox 0x0009A3A0, EAX target, EBX output
// `release` 0 makes the body hold its latches and live attachments; 1 lets them go.
void hold_runtime_jump_on_body(u32 release, std::span<u8> state,
    RuntimeJumpOnStateContext& context); // Xbox 0x0009A420, ECX release, EAX state
bool step_runtime_jump_on_leap(std::span<u8> state, RuntimeJumpOnStateContext& context); // Xbox 0x0009A870, EBX body
bool step_runtime_jump_off_leap(std::span<u8> state, RuntimeJumpOnStateContext& context); // Xbox 0x0009A9C0, EDI body
void start_runtime_jump_on_leap(std::span<u8> state, RuntimeJumpOnStateContext& context); // Xbox 0x0009AF60, EAX actor
void start_runtime_jump_off_leap(Actor* ridden, std::span<u8> state,
    RuntimeJumpOnStateContext& context); // Xbox 0x0009B010, EAX ridden
void set_runtime_jump_on_phase(u32 phase, std::span<u8> state,
    RuntimeJumpOnStateContext& context); // Xbox 0x0009B160, ESI state
void end_runtime_jump_on_state(RuntimeJumpOnStateContext& context); // Xbox 0x0009B290
void release_runtime_jump_on_claim(std::span<u8> state,
    RuntimeJumpOnStateContext& context); // Xbox 0x0009B300, EDX state, EDI actor
void start_runtime_ride(std::span<u8> ride, RuntimeJumpOnStateContext& context); // Xbox 0x000994E0, ECX ride, EAX actor
Actor* end_runtime_ride(std::span<u8> ride, RuntimeJumpOnStateContext& context); // Xbox 0x00099550, EBX actor
bool tick_runtime_ride(std::span<u8> state, u32 keep_riding,
    RuntimeJumpOnStateContext& context); // Xbox 0x00099610, EBX ride
void message_runtime_ride(std::span<u8> ride, std::span<const u32> message,
    RuntimeJumpOnStateContext& context); // Xbox 0x00099940, ECX ride, EAX actor, ESI message
u32 query_runtime_ride_host(u32 value, RuntimeJumpOnStateContext& context); // Xbox 0x00099A20, EAX body
void reset_runtime_ride_wobble(std::span<u8> ride, RuntimeJumpOnStateContext& context); // Xbox 0x00099A60, ECX ride
void steer_runtime_ride_host(std::span<u8> ride, RuntimeJumpOnStateContext& context); // Xbox 0x00099AD0, EAX actor
void roll_runtime_ride_idle(std::span<u8> ride, RuntimeJumpOnStateContext& context); // Xbox 0x00099C40
void set_runtime_ride_phase(std::span<u8> ride, u32 phase,
    RuntimeJumpOnStateContext& context); // Xbox 0x00099CA0, EDI ride
void attach_runtime_ride(RuntimeJumpOnStateContext& context); // Xbox 0x00099DF0, EBX actor
Actor* detach_runtime_ride(RuntimeJumpOnStateContext& context); // Xbox 0x00099EE0, EAX actor
void strike_runtime_ride_host(std::span<u8> ride, RuntimeJumpOnStateContext& context); // Xbox 0x00099F60, EAX actor
void track_runtime_ride_input(std::span<u8> ride, RuntimeJumpOnStateContext& context); // Xbox 0x0009A060, EAX actor
// A running timer counts down by the frame step; when it runs out it is redrawn between +4 and +8 and true returned.
bool tick_runtime_random_timer(std::span<u8> timer, u32& random, const RuntimeActorClock& clock); // Xbox 0x001050A0, ECX timer
// The leap record (0x50 bytes): +0 the target euler, +0xC the start euler, +0x18 the current one, +0x24 the start
// point, +0x30 the end point, +0x3C the current one, +0x48 the length and +0x4C the time left.
void start_runtime_leap(std::span<u8> leap, const Vec3& target_euler, const Vec3& start_euler, const Vec3& start,
    const Vec3& end, float length); // Xbox 0x00103650
bool step_runtime_leap(std::span<u8> leap, float step); // Xbox 0x001036D0, ESI leap
// The target's body +0x718 takes the rider when it has none and is not knocked away (bit 0x1B).
bool claim_runtime_body_rider(std::span<u8> body, const EntityBitBlock& bits, u32 rider); // Xbox 0x000265A0, EAX body
bool claim_runtime_rider(const RuntimeJumpOnPeer* target, u32 rider); // Xbox 0x00138140, EAX target
bool release_runtime_rider(const RuntimeJumpOnPeer* target, u32 rider); // Xbox 0x00138190, EAX target, EDX rider
// A free point on the actor's floor between the two bodies' reach (and `from`'s held sword) and `extra` beyond it.
bool find_runtime_jump_landing(const RuntimeJumpOnPeer& from, float extra, Vec3& output,
    RuntimeJumpOnStateContext& context); // Xbox 0x00138220, EAX from, the actor on the stack
// The intro "intros_imphat" once (story bit 3), when an intro may show.
u32 queue_runtime_imp_hat_intro(u8& story_byte, RuntimeIntroNotices& context); // Xbox 0x0013FE60
// Type 0x79 (base 9) with its lifecycle; `context_of` gives a state's views by its body address (state +8).
void install_runtime_jump_on_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeJumpOnStateContext&(u32 body_address)> context_of);
}
