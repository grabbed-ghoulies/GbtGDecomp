#pragma once
#include "weapon_fields.h"

#include "weapon_actor.h"

namespace gbtg
{
// entityWeaponGun, factory type 0xA4 (getter 0x000CA980, vtable fill 0x000CA990) over the weapon (type 0x0B): a
// weapon that fires from its muzzle locator (model binding kind 3) toward its aim locator (kind 4). While it is held
// (state 3) firing drains its charge (+0xED8) and an empty gun waits 5 s before it recharges. Message 0xA2 starts
// firing and 0xA3 stops it. A gun class fills six more slots: +0x4C fire start, +0x50 fire end, +0x54 the update
// with the frame step, +0x58 the draw at the muzzle, +0x5C the fire tick and +0x60 the hit query.
namespace gun_weapon_parameter
{
constexpr RetailField<std::array<char, 0x80>, 0x5F8> name{};
constexpr u32 kSize = 0x678;
static_assert(retail_fits(kSize, name));
} // namespace gun_weapon_parameter
struct GunWeaponState
{
    WeaponBase base{}; // +0
    u32 firing = 0; // +0xE54
    std::array<char, 0x80> name{}; // +0xE58, parameters +0x5F8
    float charge = 0.f; // +0xED8, 0..1
    float empty_delay = 0.f; // +0xEDC, seconds before an emptied gun recharges
    float recharge = 0.f; // +0xEE0, per second, once the delay has run out
    float drain = 0.f; // +0xEE4, per second while firing
    u32 unk_EE8 = 0; // +0xEE8, message 0xA1's word
    u32 muzzle = 0; // +0xEEC, set when the kind 3 locator was found (0x000408D0)
    u32 aim = 0; // +0xEF0, set when the kind 4 locator was found
    u32 unk_EF4 = 0; // +0xEF4, message 0x5A sets it, 0x92 clears it; the contact counts while clear
    u32 hits = 0; // +0xEF8, hits reported since firing started
};
static_assert(sizeof(GunWeaponState) == 0xEFC);
void gun_weapon_object_sizes(ObjectSizes& sizes); // Xbox 0x000CA960
bool initialize_runtime_gun_weapon(RuntimeWeaponActor& weapon, std::span<const u8> parameters); // Xbox 0x000CA9F0
void dispatch_runtime_gun_weapon_message(RuntimeWeaponActor& weapon, std::span<u32> message); // Xbox 0x000CAB40
void update_runtime_gun_weapon_state(RuntimeWeaponActor& weapon, u16 state); // Xbox 0x000CAA80, vtable +0x44
// The weapon draw 0x001459C0 runs first; this is the rest of 0x000CAD90: pass bit 2 draws the class's muzzle effects.
void draw_runtime_gun_weapon_effects(RuntimeWeaponActor& weapon, u32 pass); // Xbox 0x000CAD90
void enter_runtime_gun_weapon_state(RuntimeWeaponActor& weapon, u16 state); // Xbox 0x000CAE90, vtable +0x3C
void leave_runtime_gun_weapon_state(RuntimeWeaponActor& weapon, u16 state); // Xbox 0x000CAEB0, vtable +0x40
void late_update_runtime_gun_weapon_state(RuntimeWeaponActor& weapon, u16 state); // Xbox 0x000CAF10, vtable +0x48
// The muzzle's world point and the unit direction toward the aim locator; false without the locators.
bool runtime_gun_muzzle(RuntimeWeaponActor& weapon, Vec3& point, Vec3& direction); // Xbox 0x000CB330, ECX actor, EAX point
// While not firing, an emptied gun counts its delay down and then recharges; +0x93C follows the charge.
void recharge_runtime_gun_weapon(RuntimeWeaponActor& weapon, const Actor* holder); // Xbox 0x000CB200, EAX holder, ESI actor
// The charge ran out while firing: firing stops and the gun waits 5 s.
void exhaust_runtime_gun_weapon(RuntimeWeaponActor& weapon, const Vec3& point,
    const Vec3& direction); // Xbox 0x000CAE00, ESI actor
// The class's hit query (+0x60) with the holder's shapes left out, each hit reported (0x00141C00) and the list freed.
void report_runtime_gun_weapon_hits(RuntimeWeaponActor& weapon, Actor& holder, const Vec3& point,
    const Vec3& direction); // Xbox 0x000CB0B0, EDX point
// Actor +0x99C 0x000CB040: a contact reports kind 7, strength 0x3A, style 0x68.
u32 test_runtime_gun_contact(RuntimeWeaponActor& weapon, RuntimeSegmentContactActor& object, const Vec3& point,
    GhoulySpawnPool* contacts, const Vec3& velocity, u16 word); // Xbox 0x000CB040
}
