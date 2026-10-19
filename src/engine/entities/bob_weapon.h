#pragma once

#include "weapon_actor.h"

namespace gbtg
{
// entityWeaponBob, factory type 0xA0 (getter 0x000C5BB0, vtable 0x005060B8) over the weapon (type 0x0B). Like the
// chair it keeps the weapon update 0x0014A2E0 (+0x10), the weapon draw 0x001459C0 (+0x14) and the empty 0x001628B0
// (+0x48).
// Bytes: +0x99C the contact test 0x000C61D0, +0xE54 parameters +0x5F8 (a count messages 0x86 / 0x87 move),
// +0xE5C / +0xEDC / +0xF60 three 0x80-byte names (parameters +0x5FC, +0x67C, +0x6FC; +0xF60 is the cutscene),
// +0xF5C the state 7 timer, +0xFE0 the cutscene instance, +0xFE4.
void bob_weapon_object_sizes(ObjectSizes& sizes); // Xbox 0x000C5B90
bool initialize_runtime_bob_weapon(RuntimeWeaponActor& weapon, std::span<const u8> parameters); // Xbox 0x000C5C20
void destroy_runtime_bob_weapon(RuntimeWeaponActor& weapon); // Xbox 0x000C5D30
void dispatch_runtime_bob_weapon_message(RuntimeWeaponActor& weapon, std::span<u32> message); // Xbox 0x000C5EB0
void enter_runtime_bob_weapon_state(RuntimeWeaponActor& weapon, u16 state,
    u16 previous); // Xbox 0x000C6100, vtable +0x3C
void leave_runtime_bob_weapon_state(RuntimeWeaponActor& weapon, u16 state, u16 next); // Xbox 0x000C61B0, vtable +0x40
void update_runtime_bob_weapon_state(RuntimeWeaponActor& weapon, u16 state); // Xbox 0x000C5D50, vtable +0x44
}
