#pragma once

#include "weapon_actor.h"

namespace gbtg
{
// entityWeaponBob, factory type 0xA0 (getter 0x000C5BB0, vtable 0x005060B8) over the weapon (type 0x0B). Like the
// chair it keeps the weapon update 0x0014A2E0 (+0x10), the weapon draw 0x001459C0 (+0x14) and the empty 0x001628B0
// (+0x48).
// Bytes: +0x99C the contact test 0x000C61D0, +0xE54 parameters +0x5F8 (a count messages 0x86 / 0x87 move),
// +0xE5C / +0xEDC / +0xF60 three 0x80-byte names (parameters +0x5FC, +0x67C, +0x6FC: an animation, its events and
// the cutscene),
// +0xF5C the state 7 timer, +0xFE0 the cutscene instance, +0xFE4.
// The bob's own parameters (0x7FC bytes in all) and instance fields (0xFE8 bytes in all, 0x000C5B90), after the
// weapon's. +0xFE0 the cutscene instance is RuntimeWeaponActor::cutscene.
namespace bob_weapon_parameter
{
constexpr RetailField<u32, 0x5F8> holders{};
constexpr RetailField<std::array<char, 0x80>, 0x5FC> animation{};
constexpr RetailField<std::array<char, 0x80>, 0x67C> animation_events{};
constexpr RetailField<std::array<char, 0x80>, 0x6FC> cutscene{};
constexpr u32 kSize = 0x7FC;
static_assert(retail_fits(kSize, cutscene));
} // namespace bob_weapon_parameter
namespace bob_weapon_field
{
// A count messages 0x86 / 0x87 (kind 0xD) move: up while the bob is in states 3 or 0x10, down in 0x10.
constexpr RetailField<u32, 0xE54> holders{};
constexpr RetailField<u32, 0xE58> hits{}; // contacts the bob reported (0x000C61D0); zero counts the first
constexpr RetailField<std::array<char, 0x80>, 0xE5C> animation{}; // the animation states 7 and 14 play (0x000C62C0)
constexpr RetailField<std::array<char, 0x80>, 0xEDC> animation_events{}; // that animation's events
constexpr RetailField<float, 0xF5C> state_7_timer{}; // 1 s while DAT_004E1C34 > -1, then ticking down (0x00042360)
constexpr RetailField<std::array<char, 0x80>, 0xF60> cutscene{}; // the cutscene 0x000C5C20 loads (0x0004D680)
constexpr RetailField<u32, 0xFE4> unk_FE4{}; // message 0x87 needs it 0 and writes 0; nothing here sets it
constexpr u32 kSize = 0xFE8;
static_assert(retail_fits(kSize, unk_FE4));
} // namespace bob_weapon_field
void bob_weapon_object_sizes(ObjectSizes& sizes); // Xbox 0x000C5B90
bool initialize_runtime_bob_weapon(RuntimeWeaponActor& weapon, std::span<const u8> parameters); // Xbox 0x000C5C20
void destroy_runtime_bob_weapon(RuntimeWeaponActor& weapon); // Xbox 0x000C5D30
void dispatch_runtime_bob_weapon_message(RuntimeWeaponActor& weapon, std::span<u32> message); // Xbox 0x000C5EB0
// The bob's contact test (+0x99C): the weapon contact report with the bob's kind bytes, counting its hits.
u32 test_runtime_bob_contact(RuntimeWeaponActor& weapon, RuntimeSegmentContactActor& object, const Vec3& point,
    GhoulySpawnPool* contacts, const Vec3& velocity, u16 word); // Xbox 0x000C61D0
// Message 0x95 (sent by the swing state 0x00098E00 as it takes the bob): the holder count drops; at zero the bob's
// model anchors are blocked (0x00147860) and +0x93C cleared as 0x00146230 does.
void release_runtime_bob_weapon_holder(RuntimeWeaponActor& weapon); // Xbox 0x000C6250, EAX actor
// State 7's end (its timer run out): the +0xE5C animation with the +0xEDC events, callout 0xD and the weapon's
// points (category 1).
void finish_runtime_bob_weapon_rest(RuntimeWeaponActor& weapon); // Xbox 0x000C62C0, EAX actor
// State 14's start: the +0xE5C animation played backwards from where it stands, one more holder, +0xFE4 set and
// callout 0x4B.
void start_runtime_bob_weapon_rest(RuntimeWeaponActor& weapon); // Xbox 0x000C63A0, EAX actor
void enter_runtime_bob_weapon_state(RuntimeWeaponActor& weapon, u16 state,
    u16 previous); // Xbox 0x000C6100, vtable +0x3C
void leave_runtime_bob_weapon_state(RuntimeWeaponActor& weapon, u16 state, u16 next); // Xbox 0x000C61B0, vtable +0x40
void update_runtime_bob_weapon_state(RuntimeWeaponActor& weapon, u16 state); // Xbox 0x000C5D50, vtable +0x44
}
