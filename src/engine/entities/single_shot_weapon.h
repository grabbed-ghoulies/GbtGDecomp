#pragma once

#include "gun_weapon.h"

namespace gbtg
{
// entityWeaponSingleShot, factory type 0xA5 (getter 0x000C8E00, vtable 0x00506210; the factory table's row 0xA4
// carries the name) over the gun (type 0xA4): the blunderbuss. Each fire (+0x4C) spawns the projectile template named
// at +0xEFC from the muzzle (pulled back to the holder's side of any wall), owned by the holder, flying toward the
// message's aim point (or along the muzzle when that lies behind), shows callout 0x2B there, spends 0.15 of the
// charge and tells the holder (message 0x3E). It does not recharge (+0xEE4 0) and leaves the gun's update (+0x54),
// draw (+0x58) and hit query (+0x60) unset; its fire end (+0x50) and fire tick (+0x5C) are the empty 0x001628B0.
// Message 0x9A answers 1 in its second word.
namespace single_shot_weapon_parameter
{
constexpr RetailField<std::array<char, 0x80>, 0x678> projectile{};
constexpr u32 kSize = 0x6F8;
static_assert(retail_fits(kSize, projectile));
} // namespace single_shot_weapon_parameter
struct SingleShotWeaponState
{
    GunWeaponState base{}; // +0
    std::array<char, 0x80> projectile{}; // +0xEFC, parameters +0x678
};
static_assert(sizeof(SingleShotWeaponState) == 0xF7C);
void single_shot_weapon_object_sizes(ObjectSizes& sizes); // Xbox 0x000C8DE0
bool initialize_runtime_single_shot_weapon(RuntimeWeaponActor& weapon,
    std::span<const u8> parameters); // Xbox 0x000C8E70
void dispatch_runtime_single_shot_message(RuntimeWeaponActor& weapon, std::span<u32> message); // Xbox 0x000C8EC0
// Vtable +0x4C (0x000C8EE0 jumps here): `aim` is message 0xA2's point (its +8 words).
void fire_runtime_single_shot(RuntimeWeaponActor& weapon, const Vec3& point, const Vec3& direction,
    const Vec3& aim); // Xbox 0x000C8EF0
}
