#pragma once

#include "gun_weapon.h"

namespace gbtg
{
// entityWeaponExtinguisher, factory type 0xA3 (getter 0x000C7B70, vtable 0x005061A8) over the gun (type 0xA4): the
// fire extinguisher. While it fires, its spray (the parameters' AID, +0x678, drawn as billboards) is emitted 30
// particles a second into a block (+0xF20, 0x000C7D10); when firing stops the block moves to one of eight trailing
// blocks (+0xF00) that die out on their own. Its reach (+0xF28) grows while firing and shrinks after. Each frame the
// spray is tested against the world: particles that meet a wall bounce off it, and five growing spheres along the
// spray report the actors they touch to the gun (0x000CB0B0). It recharges at 0.075 a second and drains at a rate
// that follows the gun's +0xEE8 word.
namespace extinguisher_weapon_parameter
{
constexpr RetailField<std::array<char, 0x80>, 0x678> spray{};
constexpr u32 kSize = 0x6F8;
static_assert(retail_fits(kSize, spray));
} // namespace extinguisher_weapon_parameter
namespace extinguisher_weapon_field
{
constexpr RetailField<u32, 0xEFC> spray{}; // set when the spray AID was loaded
constexpr u32 kTrails = 0xF00; // Eight trailing spray blocks, 4 apart (host: their pools are kept natively).
constexpr RetailField<u32, 0xF20> active{}; // set while the firing spray's block (0x000C7D10) exists
constexpr RetailField<float, 0xF24> emission{}; // seconds of spray not yet emitted, a particle per 1/30 s
constexpr RetailField<float, 0xF28> reach{}; // 0.01 (DAT_0032B124) .. 1: grows 1.98 a second while firing
constexpr u32 kSize = 0xF2C;
static_assert(retail_fits(kSize, reach));
} // namespace extinguisher_weapon_field
// One 0x38-byte spray particle.
struct RuntimeSprayParticle
{
    Vec3 position; // +0
    Vec3 velocity; // +0xC
    float life; // +0x18, 0.25..0.45 s
    float age; // +0x1C
    u8 alpha; // +0x20
    u8 alpha_start; // +0x21
    u8 alpha_end; // +0x22
    u8 bounced; // +0x23, set once it bounced off a wall
    float size; // +0x24
    float size_start; // +0x28
    float size_end; // +0x2C
    float spin; // +0x30, radians a second
    float rotation; // +0x34
};
static_assert(sizeof(RuntimeSprayParticle) == 0x38);
// One hit of the spray's query (0x20 bytes in retail: the actor, its copied contacts, the point and the velocity).
struct RuntimeSprayHit
{
    entity_detail::SegmentProbe* object; // +0, the probe of the actor touched
    GhoulySpawnPool* contacts; // +4, 0x000FCC90's copy
    Vec3 point; // +8
    Vec3 velocity; // +0x14
};
void extinguisher_weapon_object_sizes(ObjectSizes& sizes); // Xbox 0x000C7B50
bool initialize_runtime_extinguisher_weapon(RuntimeWeaponActor& weapon,
    std::span<const u8> parameters); // Xbox 0x000C7BE0
void destroy_runtime_extinguisher_weapon(RuntimeWeaponActor& weapon); // Xbox 0x000C7C60
// Vtable +0x4C: firing starts with an empty spray block.
void start_runtime_extinguisher_spray(RuntimeWeaponActor& weapon); // Xbox 0x000C7D10
// Vtable +0x50: firing ends; the spray block becomes a trailing block (0x000C8D30).
void end_runtime_extinguisher_spray(RuntimeWeaponActor& weapon); // Xbox 0x000C7D50
void retire_runtime_extinguisher_spray(RuntimeWeaponActor& weapon); // Xbox 0x000C8D30, ESI actor
// Vtable +0x54: the drain follows +0xEE8, and each spray block advances by the step, freed once it has died out.
void update_runtime_extinguisher_spray(RuntimeWeaponActor& weapon, float step); // Xbox 0x000C7D60
// Each particle moves and ages; the dead ones are removed. Whether any are left.
bool step_runtime_spray_block(GhoulySpawnPool& block, float step); // Xbox 0x000C7E40, EDI block
// Vtable +0x58: each spray block's particles are drawn.
void draw_runtime_extinguisher_spray(RuntimeWeaponActor& weapon); // Xbox 0x000C8C70
void draw_runtime_spray_block(RuntimeWeaponActor& weapon, const GhoulySpawnPool& block); // Xbox 0x000C8CB0, ECX block, EBX actor
// Vtable +0x5C: the reach follows `firing`, and while firing particles are emitted from `point` along `direction`.
void emit_runtime_extinguisher_spray(RuntimeWeaponActor& weapon, u32 firing, const Vec3& point,
    const Vec3& direction); // Xbox 0x000C8030
// Vtable +0x60: every spray block's query; the hits (RuntimeSprayHit records), or null.
GhoulySpawnPool* query_runtime_extinguisher_spray(RuntimeWeaponActor& weapon, const Vec3& point,
    const Vec3& direction); // Xbox 0x000C8590
void query_runtime_spray_block(RuntimeWeaponActor& weapon, GhoulySpawnPool& block, const Vec3& point,
    const Vec3& direction, GhoulySpawnPool*& hits); // Xbox 0x000C8610, EAX point
}
