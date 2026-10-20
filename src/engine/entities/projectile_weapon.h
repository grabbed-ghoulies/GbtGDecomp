#pragma once

#include "weapon_actor.h"

namespace gbtg
{
// entityWeaponProjectileLarge, factory type 0xA9 (getter 0x000CD640, vtable 0x005063E8) over the weapon (type 0x0B):
// a weapon thrown in a high arc, such as the snooker room's trophy. Like the chair it keeps the weapon update
// 0x0014A2E0 (+0x10), the weapon draw 0x001459C0 (+0x14) and the empty 0x001628B0 (+0x48).
// Bytes: +0x99C the contact test 0x000CE5D0, +0xE54..+0xE5C message 0xA5's words, +0xE74 the throw delay,
// +0xE7C the collision override pool, +0xE84, +0xE88 the hits counted, +0xEA4 the catcher (message 0xA0),
// +0xEA8 / +0xEB4 the euler and position at the catch, +0xEC0..+0xED0 the catch's offset and words,
// +0xED4..+0xEDC parameters +0x5F8..+0x600.
// The large projectile's own parameters (0x604 bytes in all) and instance fields (0xF08 bytes in all, 0x000CD620),
// after the weapon's. +0xE7C the collision override pool is RuntimeWeaponActor::collision_overrides.
namespace large_projectile_parameter
{
constexpr RetailField<float, 0x5F8> hold_turn{};
constexpr RetailField<float, 0x5FC> hold_forward{};
constexpr RetailField<float, 0x600> hold_up{};
} // namespace large_projectile_parameter
namespace large_projectile_field
{
constexpr RetailField<Vec3, 0xE54> throw_target{}; // message 0xA5's words; the launch heads at it (0x000CE020)
constexpr RetailField<float, 0xE60> fall_speed{}; // the vertical speed: -437 at launch (DAT_0032B65C)
constexpr RetailField<float, 0xE64> heading{}; // the launch's yaw toward throw_target
constexpr RetailField<Vec3, 0xE68> spin{}; // turns per second; 267 degrees about x at launch, kicked at each bounce
// The shatter: 0.6 s (DAT_0032B248), ticking down; its reach grows as it runs (0x000CEC70).
constexpr RetailField<float, 0xE74> shatter_timer{};
constexpr RetailField<float, 0xE78> shatter_scale{}; // 2 for a scripted player hold, else 1
constexpr RetailField<u32, 0xE80> shatter_shown{}; // set once the shatter's effect callouts have run (0x000CEB10)
constexpr RetailField<i16, 0xE84> bounces_left{}; // 3 at launch
constexpr RetailField<u8, 0xE86> settled{}; // 0x000CE290 sets it; its old value is the result
constexpr RetailField<u32, 0xE88> hits{}; // the shatter's hits counted (0x00141A30)
constexpr RetailField<Vec3, 0xE8C> rest_euler{}; // message 0x9D: the root joint's world euler (0x000CE660)
constexpr RetailField<Vec3, 0xE98> rest_position{}; // and the actor's position less that joint's turned point
constexpr RetailField<u32, 0xEA4> catcher{}; // the holder that asked (message 0xA0)
constexpr RetailField<Vec3, 0xEA8> catch_euler{}; // the euler and position when caught
constexpr RetailField<Vec3, 0xEB4> catch_position{};
constexpr RetailField<Vec3, 0xEC0> hold_offset{}; // the hold point in the holder's space; x plus hold_forward
constexpr RetailField<float, 0xECC> rise_elapsed{}; // state 0xF's rise toward the hold point
constexpr RetailField<float, 0xED0> rise_duration{};
constexpr RetailField<float, 0xED4> hold_turn{}; // large_projectile_parameter::hold_turn
constexpr RetailField<float, 0xED8> hold_forward{};
constexpr RetailField<float, 0xEDC> hold_up{};
constexpr RetailRecord<0xEE0, 36> command_attachments{}; // the attachments 0x0013B8B0 records at launch
constexpr RetailField<float, 0xF04> trail_timer{}; // 0x0013B030's trail clock
constexpr u32 kSize = 0xF08;
static_assert(retail_fits(kSize, trail_timer));
} // namespace large_projectile_field
void large_projectile_weapon_object_sizes(ObjectSizes& sizes); // Xbox 0x000CD620
bool initialize_runtime_large_projectile(RuntimeWeaponActor& weapon,
    std::span<const u8> parameters); // Xbox 0x000CD6B0
void destroy_runtime_large_projectile(RuntimeWeaponActor& weapon); // Xbox 0x000CDAA0
void dispatch_runtime_large_projectile_message(RuntimeWeaponActor& weapon,
    std::span<u32> message); // Xbox 0x000CDBE0
void enter_runtime_large_projectile_state(RuntimeWeaponActor& weapon, u16 state,
    u16 previous); // Xbox 0x000CD700, vtable +0x3C
void leave_runtime_large_projectile_state(RuntimeWeaponActor& weapon, u16 state,
    u16 next); // Xbox 0x000CD8A0, vtable +0x40
// Degrees to angles in [0, 2pi) (ESI degrees, EDI the output).
Vec3 runtime_degrees_to_turns(const Vec3& degrees); // Xbox 0x001058E0
// State 2's start: the spin (267 degrees about x), the fall (-437), three bounces and 167 toward +0xE54.
void launch_runtime_large_projectile(RuntimeWeaponActor& weapon); // Xbox 0x000CE020
void bounce_runtime_large_projectile_velocity(RuntimeWeaponActor& weapon, float pitch_degrees,
    float keep); // Xbox 0x000CE150, EAX the bounce record, EDI actor
// State 2 entered again: the spin slows and kicks, the velocity turns up.
void bounce_runtime_large_projectile(RuntimeWeaponActor& weapon); // Xbox 0x000CE1F0, EBX actor
void spin_runtime_large_projectile(RuntimeWeaponActor& weapon); // Xbox 0x000CE510, EBX actor
// 3 when it hits something steep (or starts inside something), 2 on a floor, 1 when the move is clear.
i32 collide_runtime_large_projectile(RuntimeWeaponActor& weapon, Vec3& last, Vec3& next,
    float radius); // Xbox 0x000CE290, EAX actor, ECX the last position
i32 step_runtime_large_projectile(RuntimeWeaponActor& weapon, bool last_bounce); // Xbox 0x000CE380, ECX actor
void fly_runtime_large_projectile(RuntimeWeaponActor& weapon); // Xbox 0x000CDAD0, EAX actor
// The shatter: scale +0xE78 (2 for a scripted player hold, else 1) and timer +0xE74 at 0.6, the holder kept out.
void start_runtime_large_projectile_shatter(RuntimeWeaponActor& weapon); // Xbox 0x000CEBD0, ESI actor
// The shatter's effect callouts (10, or 11 when scaled up) at the actor's position; sets +0xE80.
void show_runtime_large_projectile_shatter(RuntimeWeaponActor& weapon); // Xbox 0x000CEB10, EAX actor
// The shatter's growing reach hits every actor in it once; true when the timer runs out.
bool update_runtime_large_projectile_shatter(RuntimeWeaponActor& weapon); // Xbox 0x000CEC70, EAX actor
// Message 0x9D: the root joint's world euler (EBX, +0xE8C) and the actor's position less that joint's point turned
// by it (+0xE98).
void record_runtime_large_projectile_root(RuntimeWeaponActor& weapon, std::span<u8> euler,
    std::span<u8> offset); // Xbox 0x000CE660, EAX actor
// State 7: back to the pose message 0x9D recorded (+0xE98 position, +0xE8C euler).
void restore_runtime_large_projectile_root(RuntimeWeaponActor& weapon); // Xbox 0x000CE7E0, EBX actor
// State 0xF: the projectile rises from where it was taken (+0xEB4, euler +0xEA8) to the holder's hold point (+0xEC0
// in the holder's space, +0xEDC up, +0xED4 turned) over +0xED0, then tells the holder (+0xEA4) with {0x5C, itself}.
void lift_runtime_large_projectile(RuntimeWeaponActor& weapon); // Xbox 0x000CE810, EAX actor
void update_runtime_large_projectile_state(RuntimeWeaponActor& weapon, u16 state); // Xbox 0x000CDA10, vtable +0x44
void start_runtime_large_projectile_idle(RuntimeWeaponActor& weapon); // Xbox 0x000CDF40, EAX actor
void show_runtime_large_projectile_holder(RuntimeWeaponActor& weapon); // Xbox 0x000CEEC0, EAX actor
void mark_runtime_weapon_player_category(RuntimeWeaponActor& weapon); // Xbox 0x001454B0, ECX actor
}
