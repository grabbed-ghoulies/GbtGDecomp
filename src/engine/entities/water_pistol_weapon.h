#pragma once

#include "gun_weapon.h"

namespace gbtg
{
// entityWeaponWaterPistol, factory type 0xA6 (getter 0x000C9210, vtable 0x00506278; the factory table's row 0xA5
// carries the name) over the gun (type 0xA4). While it fires, a droplet is emitted every 1/30 s into a block (+0xF34,
// 0x000C9410) along the aim at 150 x +0xEE8 + 50 a second; when firing stops the block moves to one of twelve
// trailing blocks (+0xF04, 0x000CA3D0). Droplets fall at 100 a second squared and splash ("expire" effect) after 2 s.
// The hit query sweeps each droplet's last move against the world: a hit reports the actor touched, splashes ("hit"
// effect) and drops the droplets behind it, leaving "trail" effects between them. The droplets are drawn as one strip
// with the parameters' texture (+0x678), and the "muzzle" effect follows the muzzle while firing.
namespace water_pistol_weapon_parameter
{
constexpr RetailField<std::array<char, 0x80>, 0x678> texture{};
constexpr RetailField<std::array<char, 0x80>, 0x6F8> hit{}; // Copied to +0xFC0
constexpr RetailField<std::array<char, 0x80>, 0x778> expire{}; // Copied to +0x1040
constexpr RetailField<std::array<char, 0x80>, 0x7F8> muzzle{}; // Copied to +0x10C0
constexpr RetailField<std::array<char, 0x80>, 0x878> trail{}; // Copied to +0x1140
constexpr u32 kSize = 0x8F8;
static_assert(retail_fits(kSize, trail));
} // namespace water_pistol_weapon_parameter
struct WaterPistolWeaponState
{
    GunWeaponState base{}; // +0
    u32 muzzle_effect = 0; // +0xEFC, set while the muzzle effect lives (host: its node is native)
    u32 texture = 0; // +0xF00, set when the texture AID was loaded
    std::array<u32, 12> trails{}; // +0xF04, twelve trailing droplet blocks (host: their pools are kept natively)
    u32 active = 0; // +0xF34, set while the firing droplet block exists
    float emission = 0.f; // +0xF38, seconds of fire not yet emitted, a droplet per 1/30 s
    u32 strip = 0; // +0xF3C, set once the draw's point list exists
    std::array<u8, 0x80> unk_F40{}; // +0xF40, never read or written by translated code
    std::array<char, 0x80> hit_effect{}; // +0xFC0
    std::array<char, 0x80> expire_effect{}; // +0x1040
    std::array<char, 0x80> muzzle_name{}; // +0x10C0
    std::array<char, 0x80> trail_effect{}; // +0x1140
};
static_assert(sizeof(WaterPistolWeaponState) == 0x11C0);
// One 0x28-byte droplet.
struct RuntimeWaterDroplet
{
    Vec3 position; // +0
    Vec3 previous; // +0xC, the position before the last step
    Vec3 velocity; // +0x18
    float age; // +0x24
};
static_assert(sizeof(RuntimeWaterDroplet) == 0x28);
void water_pistol_weapon_object_sizes(ObjectSizes& sizes); // Xbox 0x000C91F0
bool initialize_runtime_water_pistol_weapon(RuntimeWeaponActor& weapon,
    std::span<const u8> parameters); // Xbox 0x000C9280
void destroy_runtime_water_pistol_weapon(RuntimeWeaponActor& weapon); // Xbox 0x000C9330
// Vtable +0x4C: firing starts with an empty droplet block and the muzzle effect at the point.
void start_runtime_water_pistol_fire(RuntimeWeaponActor& weapon, const Vec3& point,
    const Vec3* direction); // Xbox 0x000C9410
// Vtable +0x50: firing ends; the "expire" effect splashes at the point and the muzzle effect ends.
void end_runtime_water_pistol_fire(RuntimeWeaponActor& weapon, const Vec3& point); // Xbox 0x000C9540
// The firing block becomes a trailing block, or is freed when it holds one droplet or none.
void retire_runtime_water_pistol_block(RuntimeWeaponActor& weapon); // Xbox 0x000CA3D0, ESI actor
// Vtable +0x54: the drain follows +0xEE8, and each block advances by the step, freed once it is empty.
void update_runtime_water_pistol(RuntimeWeaponActor& weapon, float step); // Xbox 0x000C95F0
// Each droplet falls and ages; one older than 2 s splashes and is removed. Whether more than one is left.
bool step_runtime_water_block(RuntimeWeaponActor& weapon, GhoulySpawnPool& block, float step); // Xbox 0x000C96E0
// Vtable +0x5C: droplets are emitted from `point` along `direction` while firing; the muzzle effect follows.
void emit_runtime_water_pistol(RuntimeWeaponActor& weapon, u32 firing, const Vec3& point,
    const Vec3& direction); // Xbox 0x000C98D0
// Vtable +0x60: the muzzle's and every block's hits (RuntimeSprayHit records), or null.
GhoulySpawnPool* query_runtime_water_pistol(RuntimeWeaponActor& weapon, const Vec3& point,
    const Vec3& direction); // Xbox 0x000C9B20
// Vtable +0x58: each block's droplets as a strip.
void draw_runtime_water_pistol(RuntimeWeaponActor& weapon, const Vec3& point); // Xbox 0x000CA1F0
}
