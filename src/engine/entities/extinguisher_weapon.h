#pragma once

#include "gun_weapon.h"

namespace gbtg
{
// entityWeaponExtinguisher, factory type 0xA3 (getter 0x000C7B70, vtable 0x005061A8) over the gun (type 0xA4): the
// fire extinguisher. Its spray (the parameters' AID, +0x678) runs in a block of 32 particles (+0xF20, 0x000C7D10) while
// it fires; a finished spray moves to one of eight trailing blocks (+0xF00) that die out on their own. It recharges
// at 0.075 a second and drains at a rate that follows the gun's +0xEE8 word.
namespace extinguisher_weapon_parameter
{
constexpr RetailField<std::array<char, 0x80>, 0x678> spray{};
constexpr u32 kSize = 0x6F8;
static_assert(retail_fits(kSize, spray));
} // namespace extinguisher_weapon_parameter
namespace extinguisher_weapon_field
{
constexpr RetailField<u32, 0xEFC> spray{}; // set when the spray AID was loaded
constexpr u32 kTrails = 0xF00; // Eight trailing spray blocks, 4 apart.
constexpr RetailField<u32, 0xF20> active{}; // the firing spray's block (0x000C7D10), or 0
constexpr RetailField<u32, 0xF24> emitted{};
constexpr RetailField<float, 0xF28> unk_F28{}; // DAT_0032B124
constexpr u32 kSize = 0xF2C;
static_assert(retail_fits(kSize, unk_F28));
} // namespace extinguisher_weapon_field
void extinguisher_weapon_object_sizes(ObjectSizes& sizes); // Xbox 0x000C7B50
bool initialize_runtime_extinguisher_weapon(RuntimeWeaponActor& weapon,
    std::span<const u8> parameters); // Xbox 0x000C7BE0
void destroy_runtime_extinguisher_weapon(RuntimeWeaponActor& weapon); // Xbox 0x000C7C60
// Vtable +0x54: the drain follows +0xEE8, and each spray block advances by the step, freed once it has died out.
void update_runtime_extinguisher_spray(RuntimeWeaponActor& weapon, float step); // Xbox 0x000C7D60
// Vtable +0x58: each spray block's particles are drawn.
void draw_runtime_extinguisher_spray(RuntimeWeaponActor& weapon); // Xbox 0x000C8C70
}
