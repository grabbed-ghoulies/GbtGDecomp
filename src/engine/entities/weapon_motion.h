#pragma once

#include "weapon_actor.h"

namespace gbtg
{
// Weapon state 2: thrown, flying under +0xC30 gravity until it lands (+0xC44, the landing contact at +0xC48).
void enter_runtime_weapon_flight(RuntimeWeaponActor& weapon); // Xbox 0x001487C0, EAX actor
void update_runtime_weapon_flight(RuntimeWeaponActor& weapon); // Xbox 0x001488D0
void leave_runtime_weapon_flight(RuntimeWeaponActor& weapon); // Xbox 0x00148850, ESI actor
// Weapon states 5 and 8: a timed hop from the actor's position to a target (+0xC9C / +0xCBC blocks: start +0,
// target +0xC, timer +0x18, bounce count +0x1C), then up to three bounces before state 16.
void start_runtime_weapon_hop(RuntimeWeaponActor& weapon, const Vec3& target); // Xbox 0x00149230, EAX actor, EBX target
void start_runtime_weapon_drop(RuntimeWeaponActor& weapon, const Vec3& target); // Xbox 0x00149580, EAX actor, EBX target
void update_runtime_weapon_hop(RuntimeWeaponActor& weapon); // Xbox 0x00148F20, EBX actor
void update_runtime_weapon_drop(RuntimeWeaponActor& weapon); // Xbox 0x00149270, EBX actor
// Weapon state 10: led by the owner at +0xD8C over the floor grid, with a path search at +0xD04.
void enter_runtime_weapon_follow(RuntimeWeaponActor& weapon); // Xbox 0x001495C0
void leave_runtime_weapon_follow(RuntimeWeaponActor& weapon); // Xbox 0x001496D0, ESI actor
}
