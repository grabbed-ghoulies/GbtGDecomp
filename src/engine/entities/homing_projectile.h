#pragma once

#include "weapon_actor.h"
#include "actor_frame_render.h"
#include "../graphics/glow_pass.h"

namespace gbtg
{
// entityWeaponProjectileHome, factory type 0xA8 (getter 0x000CC790, vtable 0x00506398; the factory table's row 0xA7
// carries the name) over the weapon (type 0x0B): a projectile thrown at its follow target (message 0xA5 into state
// 10) that the weapon's follow update (0x00149750) leads along the floor grid, its speeds from its parameters. It
// shatters (state 4) or breaks (state 9) when it meets the holder or its target, beeps (+0xEA8) while the holder's
// target is near, and when it ignores the world (+0xF58) it is drawn with a glow (0x000CD090). It keeps the weapon
// update 0x0014A2E0 (+0x10) and the empty 0x001628B0 (+0x48).
// The projectile's own parameters (0x6A4 bytes in all) and instance fields (0xF60 bytes in all, 0x000CC770), after
// the weapon's. +0xE64 the flight animation is RuntimeWeaponActor::flight_animation, +0xE8C the collision override
// pool RuntimeWeaponActor::collision_overrides.
namespace homing_projectile_parameter
{
constexpr RetailField<u32, 0x5F8> flight_animation{}; // the animation name 0x0003AAA0 looks up
constexpr RetailField<float, 0x5FC> trail_scale{};
constexpr RetailField<u32, 0x600> shatters{};
constexpr RetailField<float, 0x604> follow_slack{};
constexpr RetailField<float, 0x608> follow_speed{};
constexpr RetailField<float, 0x60C> follow_rise{};
constexpr RetailField<u32, 0x610> ghost{};
constexpr RetailField<u32, 0x614> contact_tail{};
constexpr RetailField<u32, 0x618> beeps{};
constexpr RetailField<float, 0x61C> beep_range{};
constexpr RetailField<float, 0x620> beep_interval{};
constexpr RetailRecord<0x624, 0x80> beep_sound{};
} // namespace homing_projectile_parameter
namespace homing_projectile_field
{
constexpr RetailField<u32, 0xE60> hits{}; // the contact reports' hits counted (0x00141A30)
// +0xE64 the flight animation record
constexpr RetailField<float, 0xE68> trail_scale{}; // the flight trail (0x000CC910); zero draws none
constexpr RetailField<float, 0xE6C> trail_timer{};
constexpr RetailField<Vec3, 0xE70> rest_euler{}; // message 0x9D (0x000CC010)
constexpr RetailField<Vec3, 0xE7C> rest_position{};
constexpr RetailField<float, 0xE88> shatter_timer{}; // DAT_004288F4 at the shatter's start, ticking down
// +0xE8C the collision override pool
constexpr RetailField<u32, 0xE90> shatter_shown{}; // set once the shatter's callout 10 has run
constexpr RetailField<u32, 0xE94> shatters{}; // a hit shatters (state 4) rather than breaks (state 9)
constexpr RetailField<u32, 0xE98> beeps{}; // the flight beeps near the holder's target
constexpr RetailField<float, 0xE9C> beep_timer{};
constexpr RetailField<float, 0xEA0> beep_interval{};
constexpr RetailField<float, 0xEA4> beep_range{};
constexpr RetailRecord<0xEA8, 0x80> beep_sound{}; // 0x00041D40's cue
constexpr RetailField<float, 0xF28> follow_slack{}; // the flight's +0xCEC
constexpr RetailField<float, 0xF2C> follow_speed{}; // the flight's +0xCF0
constexpr RetailField<float, 0xF30> follow_rise{}; // the flight's +0xCF4
constexpr RetailRecord<0xF34, 36> command_attachments{}; // the attachments 0x0013B8B0 records at launch
constexpr RetailField<u32, 0xF58> ghost{}; // the flight's +0xCF8: no world probes, and the glow draw
constexpr RetailField<u32, 0xF5C> contact_tail{}; // 0x00141A30's last word
constexpr u32 kSize = 0xF60;
static_assert(retail_fits(kSize, contact_tail));
} // namespace homing_projectile_field
void homing_projectile_object_sizes(ObjectSizes& sizes); // Xbox 0x000CC770
bool initialize_runtime_homing_projectile(RuntimeWeaponActor& weapon,
    std::span<const u8> parameters); // Xbox 0x000CC800
void destroy_runtime_homing_projectile(RuntimeWeaponActor& weapon); // Xbox 0x000CC8E0
void update_runtime_homing_projectile_state(RuntimeWeaponActor& weapon, u16 state); // Xbox 0x000CC970, vtable +0x44
void dispatch_runtime_homing_projectile_message(RuntimeWeaponActor& weapon,
    std::span<u32> message); // Xbox 0x000CCB70
void enter_runtime_homing_projectile_state(RuntimeWeaponActor& weapon, u16 state,
    u16 previous); // Xbox 0x000CCE00, vtable +0x3C
void leave_runtime_homing_projectile_state(RuntimeWeaponActor& weapon, u16 state,
    u16 next); // Xbox 0x000CCF60, vtable +0x40
// The class's contact test (+0x99C): report the hit and count it; true while +0x93C is clear.
u32 test_runtime_homing_projectile_contact(RuntimeWeaponActor& weapon, RuntimeSegmentContactActor& object,
    const Vec3& point, GhoulySpawnPool* contacts, const Vec3& velocity, u16 word); // Xbox 0x000CD1B0
// The draw (+0x14): with +0xF58 the frame attachments (kind 3) and, on pass 2, the actor under a 0x80FFFFFF glow;
// otherwise the plain actor draw.
u32 draw_runtime_homing_projectile(RuntimeWeaponActor& weapon, ActorDrawState& state, i32 kind,
    const ActorFrameDrawContext& attachments, SceneModelDrawContext& draw,
    RuntimeGlowPass& glow); // Xbox 0x000CD090
}
