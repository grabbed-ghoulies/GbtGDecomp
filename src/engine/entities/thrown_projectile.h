#pragma once

#include "weapon_actor.h"

namespace gbtg
{
// entityWeaponProjectile, factory type 0xA7 (getter 0x000CB570, vtable 0x00506348) over the weapon (type 0x0B): a
// weapon thrown at a point (message 0xA5) that spins as it flies, breaks or shatters where it lands, and may spawn
// the actor its parameters name there (message 0x93). Like the large projectile it keeps the weapon update 0x0014A2E0
// (+0x10), the weapon draw 0x001459C0 (+0x14) and the empty 0x001628B0 (+0x48).
// The projectile's own parameters (0x6A0 bytes in all) and instance fields (0xF70 bytes in all, 0x000CB550), after
// the weapon's. +0xF40 the collision override pool is RuntimeWeaponActor::collision_overrides.
namespace thrown_projectile_parameter
{
constexpr RetailField<u32, 0x5F8> flight_animation{}; // the animation name 0x0003AAA0 looks up
constexpr RetailRecord<0x5FC, 0x80> spawn_name{}; // copied to +0xE80
constexpr RetailField<u32, 0x67C> launch_mode{};
constexpr RetailField<Vec3, 0x680> spin_rate{};
constexpr RetailField<u32, 0x68C> flight_slide{};
constexpr RetailField<float, 0x690> trail_scale{};
constexpr RetailField<u32, 0x694> shatters{};
constexpr RetailField<u32, 0x698> flight_quiet{};
constexpr RetailField<u32, 0x69C> flight_mask{};
} // namespace thrown_projectile_parameter
namespace thrown_projectile_field
{
constexpr RetailField<u32, 0xE60> hits{}; // the contact test's hits counted (0x00141A30)
// +0xE64 the flight animation record (thrown_projectile_animation below)
constexpr RetailField<float, 0xE68> trail_scale{}; // the flight trail (0x0013B030); zero draws none
constexpr RetailField<float, 0xE6C> trail_timer{};
constexpr RetailField<u32, 0xE70> launch_mode{}; // 0 a timed arc, 1 the holder's throw (0x00140D80), else straight
constexpr RetailField<Vec3, 0xE74> throw_target{}; // message 0xA5's words
constexpr RetailRecord<0xE80, 0x80> spawn_name{}; // the actor message 0x93 spawns where it lands, when an "aid_" name
constexpr RetailField<Vec3, 0xF00> spin_rate{}; // radians per second added to +0xF0C in flight
constexpr RetailField<Vec3, 0xF0C> spin{};
constexpr RetailField<u32, 0xF18> flight_slide{}; // the flight's +0xC28: slide along slopes
constexpr RetailField<u32, 0xF1C> flight_quiet{}; // the flight's +0xC2C: zero turns the contact shapes on
constexpr RetailField<u32, 0xF20> flight_mask{}; // the flight's +0xC40 sweep mask
constexpr RetailField<Vec3, 0xF24> rest_euler{}; // message 0x9D (0x000CC010)
constexpr RetailField<Vec3, 0xF30> rest_position{};
constexpr RetailField<float, 0xF3C> shatter_timer{}; // DAT_004288DC at the shatter's start, ticking down
constexpr RetailField<u32, 0xF44> shatter_shown{}; // set once the shatter's callout 10 has run
constexpr RetailField<u32, 0xF48> shatters{}; // a landing shatters (state 4) rather than breaks (state 9)
constexpr RetailRecord<0xF4C, 36> command_attachments{}; // the attachments 0x0013B8B0 records at launch
constexpr u32 kSize = 0xF70;
static_assert(retail_fits(kSize, command_attachments));
} // namespace thrown_projectile_field
void thrown_projectile_object_sizes(ObjectSizes& sizes); // Xbox 0x000CB550
bool initialize_runtime_thrown_projectile(RuntimeWeaponActor& weapon,
    std::span<const u8> parameters); // Xbox 0x000CB5E0
void destroy_runtime_thrown_projectile(RuntimeWeaponActor& weapon); // Xbox 0x000CB6A0
void update_runtime_thrown_projectile_state(RuntimeWeaponActor& weapon, u16 state); // Xbox 0x000CB6D0, vtable +0x44
void dispatch_runtime_thrown_projectile_message(RuntimeWeaponActor& weapon,
    std::span<u32> message); // Xbox 0x000CB860
void enter_runtime_thrown_projectile_state(RuntimeWeaponActor& weapon, u16 state,
    u16 previous); // Xbox 0x000CBAF0, vtable +0x3C
void leave_runtime_thrown_projectile_state(RuntimeWeaponActor& weapon, u16 state,
    u16 next); // Xbox 0x000CBC60, vtable +0x40
// State 2's start: the launch velocity toward +0xE74 by +0xE70's mode, then the flight animation (or none).
void launch_runtime_thrown_projectile(RuntimeWeaponActor& weapon, Actor* holder); // Xbox 0x000CBD90, EBX actor
// The class's contact test (+0x99C): report the hit, count it, and while it shatters keep the actor out.
u32 test_runtime_thrown_projectile_contact(RuntimeWeaponActor& weapon, RuntimeSegmentContactActor& object,
    const Vec3& point, GhoulySpawnPool* contacts, const Vec3& velocity, u16 word); // Xbox 0x000CBF80
// The root joint's world euler and the actor's position less that joint's turned point (the matrix euler and the
// position for a model with no joints).
void record_runtime_weapon_root(const Actor& actor, Vec3& euler,
    Vec3& rest); // Xbox 0x000CC010, ECX actor, EAX euler, the stack rest
// Message 0x9D: 0x000CC010 into +0xF24 and +0xF30.
void record_runtime_thrown_projectile_root(RuntimeWeaponActor& weapon);
// State 7: back to the pose message 0x9D recorded.
void restore_runtime_thrown_projectile_root(RuntimeWeaponActor& weapon); // Xbox 0x000CC1E0, EBX actor
// State 2's spin: +0xF00 per second into +0xF0C, applied on the flight's euler.
void spin_runtime_thrown_projectile(RuntimeWeaponActor& weapon); // Xbox 0x000CC210, EBX actor
// Message 0x93 (landed): the actor named at +0xE80 is made there, and it and the holder are told {0x7A, ...}.
bool land_runtime_thrown_projectile(RuntimeWeaponActor& weapon, std::span<u32> message); // Xbox 0x000CC340, EAX actor
// Message 0x49 with a hit: unmasked, then state 4 (shatter) or 9 (break) by +0xF48.
void end_runtime_thrown_projectile_flight(RuntimeWeaponActor& weapon); // Xbox 0x000CC430, ESI actor
void start_runtime_thrown_projectile_shatter(RuntimeWeaponActor& weapon); // Xbox 0x000CC4D0, ESI actor
// The shatter's reach (DAT_004288E0..DAT_004288E4 as the timer runs) hits every actor in it once; true when done.
bool update_runtime_thrown_projectile_shatter(RuntimeWeaponActor& weapon); // Xbox 0x000CC530, EAX actor
void trail_runtime_thrown_projectile(RuntimeWeaponActor& weapon); // Xbox 0x000CC910, ECX actor
// Weapon flight set-up: the velocity, the euler, gravity and the sweep words (+0xC24..+0xC44).
void start_runtime_weapon_flight(RuntimeWeaponActor& weapon, const Vec3* velocity, const Vec3* euler, float gravity,
    u32 quiet, u32 slide, u32 mask); // Xbox 0x00148E60, EAX velocity, EDX euler, ECX quiet, ESI actor
// The launch velocity that reaches a point from another at a speed under gravity.
Vec3 runtime_ballistic_velocity(const Vec3& target, const Vec3& origin, float speed,
    float gravity); // Xbox 0x00140D80, ECX target, EDX origin, ESI the output
}
