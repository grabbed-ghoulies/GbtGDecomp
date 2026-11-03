#pragma once
#include "gbtg/retail_field.h"

#include <array>

namespace gbtg
{
// The body record (actor +0x95C, built by 0x00024C50): the most-read fields, at their retail offsets (see
// docs/RETAIL_LAYOUTS.md). +8 is the body's actor. The record's size isn't settled by the dumps, so these descriptors
// carry no size check; offsets not named here are still read literally.
namespace body_field
{
// The body's world position: 0x000412A0 copies the actor's into it.
constexpr RetailField<u32, 0x08> actor{}; // the actor the body moves, by address
constexpr RetailField<Vec3, 0x18> position{};
// Raised while something holds the body still (states raise it with the +0x309..+0x30C counters and drop it when they
// end). With it clear and move_hold_time at zero nothing moves the body (0x00024C20).
constexpr RetailField<Vec3, 0x24> unk_24{}; // the actor's position after its spheres are pushed (0x0002C2B0)
constexpr RetailField<u16, 0x30> move_holds{};
// Seconds the body is held still: decays to zero (0x00025AE0), and a random hold only ever lengthens it (0x0001AE50).
constexpr RetailField<float, 0x34> move_hold_time{};
// The point the body faces (0x000197B0's heading runs from the actor to it): a target's position (0x000412A0).
constexpr RetailField<Vec3, 0x3C> focus_point{};
// The actor the body holds or carries (a held weapon, the Baron's cane), by address.
constexpr RetailField<u32, 0x48> attached{};
// Decays to zero (0x00025AE0); message 0x4E sets it (0x000253B0), and a target request waits while it is positive
// (0x0001D180).
constexpr RetailField<float, 0x70> unk_70{};
// The body's reach, scaled by the actor's +0x38 radius (0x0001D3D0, 0x000ADCC0).
constexpr RetailField<float, 0x78> reach{};
// The current hit record (its +0x18 is the hit's turn window), by address.
constexpr RetailField<u32, 0x84> hit_record{};
constexpr RetailField<u16, 0x94> energy{};
constexpr RetailField<u16, 0x96> max_energy{}; // restored into energy on revival (0x000253B0)
constexpr RetailField<float, 0xB4> unk_B4{}; // 2 after a furnace hit while +0xA6 is set (0x00096710)
constexpr RetailField<u32, 0x158> alert_level{}; // the mind's alert, as 0x0002A650 sets it
constexpr RetailField<u32, 0x164> unk_164{}; // cleared with the turn once unk_168 is 0 (0x001346B0)
constexpr RetailField<float, 0x168> unk_168{};
// The stagger meter: a hit empties it in steps (0x0001FF60); at zero the body can be finished.
constexpr RetailField<float, 0x16C> stagger{};
constexpr RetailField<float, 0x174> unk_174{};
// The ground under the body while has_ground is set; its y is the floor height 0x0001F060 reads.
constexpr RetailField<Vec3, 0x178> ground_point{};
constexpr RetailField<u8, 0x19E> has_ground{};
constexpr RetailField<float, 0x23C> unk_23C{}; // nonzero: a hit takes the secondary action (0x00133F50)
constexpr RetailField<float, 0x240> unk_240{}; // nonzero: a hit takes the primary action (0x00133F50)
// The body's own motion; a moving body (nonzero, with flag bit 1) takes its moving key (0x000A2930, 0x0008C310).
constexpr RetailField<Vec3, 0x248> motion{};
// Counts holds on the body; the last release sends it message 0x4A.
constexpr RetailField<u32, 0x258> holds{};
// The actor that last hit the body, by address: a hit stream pushes the body away from it (0x00097500).
constexpr RetailField<u32, 0x270> attacker{};
constexpr RetailField<u32, 0x274> unk_274{}; // a player's: the actor 0x00138B40 reports; messages 0x31 and 0x6A carry it
constexpr RetailField<u32, 0x278> unk_278{}; // set when the turn resets (0x001346B0)
constexpr RetailField<u32, 0x2CC> contact_count{};
// The velocity the actor's +0x3C eases toward, at ease_rate (move action 0x5A).
constexpr RetailField<Vec3, 0x318> velocity{};
constexpr RetailField<float, 0x324> gravity{};
constexpr RetailField<float, 0x32C> step_speed{}; // times the class value (0x000259C0) is the walking speed
constexpr RetailField<float, 0x33C> heading{}; // the heading the body walks on
// ease_rate as it was before a state swapped in its own (0x000957A0, 0x0007FD30); restored when the state ends.
constexpr RetailField<float, 0x330> saved_ease_rate{};
constexpr RetailField<float, 0x334> ease_rate{};
// Raised with each ease_rate swap and lowered when it is given back.
constexpr RetailField<u32, 0x338> ease_rate_swaps{};
// Three rates the body turns by, always written together: the constructor's 0.558, 12.57, 6 (0x00024C50), the
// struggle state's parameters (0x000A2B90) and the hover state's 1.047, 2.269, 6 (0x00097B80).
constexpr RetailField<Vec3, 0x704> unk_704{}; // a held body's offset from its actor (0x0002C2B0)
constexpr RetailField<u8, 0x716> unk_716{}; // cleared while the body is held (0x0002C2B0)
constexpr RetailField<std::array<float, 3>, 0x71C> turn_rates{};
// The body's euler angles (pitch, yaw, roll): 0x00041820 copies the actor's into it.
constexpr RetailField<Vec3, 0x72C> euler{};
constexpr RetailField<u16, 0x748> action_key{}; // the action the body is running
constexpr RetailField<u32, 0x754> hit_timer_running{};
constexpr RetailField<float, 0x75C> unk_75C{};
constexpr RetailField<u32, 0x760> hit_timer{}; // the hit timer's recipient, by address; message 5 stops it
// The surface the body stands on, or surface_override while surface_override_frames counts down (0x00025AE0 clears
// the override when it reaches zero).
constexpr RetailField<u8, 0x874> surface{};
// Surfaces whose DAT_00217EC0 row scales the body's motion, one bit per surface.
constexpr RetailField<u32, 0x878> scaled_surfaces{};
constexpr RetailField<u8, 0x87E> surface_override{};
constexpr RetailField<u16, 0x880> surface_override_frames{};
// The body's height and radius scales over the actor's +0x34 and +0x38 (parameters +0x190 and +0x194, 0x00024C50).
constexpr RetailField<float, 0x884> height_scale{};
constexpr RetailField<float, 0x888> radius_scale{};
// The callout tokens control message 8 records (a count and up to eight tokens, 0x24 bytes; 0x00075C60).
constexpr RetailRecord<0x924, 0x24> control_tokens{};
constexpr RetailField<u32, 0x948> saved_energy{}; // the energy control message 4 kept, restored when it ends
constexpr RetailField<u32, 0x94C> energy_saved{};
} // namespace body_field

// The states' common pair of writes, pitch (+0x72C) and roll (+0x734) to zero: the body stands level, keeping its
// yaw.
inline void level_body_euler(std::span<u8> body)
{
    Vec3 euler = get(body, body_field::euler);
    euler.x = 0.f;
    euler.z = 0.f;
    set(body, body_field::euler, euler);
}
} // namespace gbtg
