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
void update_runtime_large_projectile_state(RuntimeWeaponActor& weapon, u16 state); // Xbox 0x000CDA10, vtable +0x44
void start_runtime_large_projectile_idle(RuntimeWeaponActor& weapon); // Xbox 0x000CDF40, EAX actor
void show_runtime_large_projectile_holder(RuntimeWeaponActor& weapon); // Xbox 0x000CEEC0, EAX actor
void mark_runtime_weapon_player_category(RuntimeWeaponActor& weapon); // Xbox 0x001454B0, ECX actor
}
