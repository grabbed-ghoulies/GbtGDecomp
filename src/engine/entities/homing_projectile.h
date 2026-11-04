#pragma once
#include "weapon_fields.h"

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
struct HomingProjectileState
{
    WeaponBase base{}; // +0
    std::array<u8, 0xC> unk_E54{}; // +0xE54, never read or written by translated code
    u32 hits = 0; // +0xE60, the contact reports' hits counted (0x00141A30)
    std::array<u8, 4> flight_animation_word{}; // +0xE64, the host keeps RuntimeWeaponActor::flight_animation
    float trail_scale = 0.f; // +0xE68, the flight trail (0x000CC910); zero draws none
    float trail_timer = 0.f; // +0xE6C
    Vec3 rest_euler{}; // +0xE70, message 0x9D (0x000CC010)
    Vec3 rest_position{}; // +0xE7C
    float shatter_timer = 0.f; // +0xE88, DAT_004288F4 at the shatter's start, ticking down
    std::array<u8, 4> collision_overrides_word{}; // +0xE8C, the host keeps RuntimeWeaponActor::collision_overrides
    u32 shatter_shown = 0; // +0xE90, set once the shatter's callout 10 has run
    u32 shatters = 0; // +0xE94, a hit shatters (state 4) rather than breaks (state 9)
    u32 beeps = 0; // +0xE98, the flight beeps near the holder's target
    float beep_timer = 0.f; // +0xE9C
    float beep_interval = 0.f; // +0xEA0
    float beep_range = 0.f; // +0xEA4
    std::array<u8, 0x80> beep_sound{}; // +0xEA8, 0x00041D40's cue
    float follow_slack = 0.f; // +0xF28, the flight's +0xCEC
    float follow_speed = 0.f; // +0xF2C, the flight's +0xCF0
    float follow_rise = 0.f; // +0xF30, the flight's +0xCF4
    std::array<u8, 0x24> command_attachments{}; // +0xF34, the attachments 0x0013B8B0 records at launch
    u32 ghost = 0; // +0xF58, the flight's +0xCF8: no world probes, and the glow draw
    u32 contact_tail = 0; // +0xF5C, 0x00141A30's last word
};
static_assert(sizeof(HomingProjectileState) == 0xF60);
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
