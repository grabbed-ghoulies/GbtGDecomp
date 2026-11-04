#pragma once
#include "weapon_fields.h"

#include "weapon_actor.h"

namespace gbtg
{
// entityWeaponContainer, factory type 0xA2 (getter 0x000C75D0, vtable 0x00506158) over the weapon (type 0x0B):
// scene 11's carried containers. Like the chair it keeps the weapon update 0x0014A2E0 (+0x10), the weapon draw
// 0x001459C0 (+0x14), an empty destructor and the empty 0x001628B0 (+0x48). It holds a name (parameters +0x5F8) that
// message 0x99 copies out while it is held; each such ask costs one of its uses (+0xE58), shown on the HUD with its
// caption (+0xE5C), and the last one drops it from the holder.
namespace container_weapon_parameter
{
constexpr RetailField<std::array<char, 0x80>, 0x5F8> contents{};
constexpr RetailField<i32, 0x678> uses{};
constexpr RetailField<u32, 0x67C> caption{};
constexpr u32 kSize = 0x680;
static_assert(retail_fits(kSize, caption));
} // namespace container_weapon_parameter
struct ContainerWeaponState
{
    WeaponBase base{}; // +0
    i32 uses = 0; // +0xE54, parameters +0x678
    i32 left = 0; // +0xE58, uses left; message 0x99 takes one
    u32 caption = 0; // +0xE5C, the HUD caption 0x0012DC80 shows the uses under
    std::array<char, 0x80> contents{}; // +0xE60, what message 0x99 copies out
};
static_assert(sizeof(ContainerWeaponState) == 0xEE0);
void container_weapon_object_sizes(ObjectSizes& sizes); // Xbox 0x000C75B0
bool initialize_runtime_container_weapon(RuntimeWeaponActor& weapon, std::span<const u8> parameters); // Xbox 0x000C7640
void dispatch_runtime_container_weapon_message(RuntimeWeaponActor& weapon, std::span<u32> message); // Xbox 0x000C7710
// While a live player holds it, the HUD shows its uses left under its caption.
void show_runtime_container_uses(RuntimeWeaponActor& weapon); // Xbox 0x000C7AF0, ECX actor
// The container is let go: its contact shapes off (message 0x92 to itself), its sound stopped, broken loose.
void drop_runtime_container_weapon(RuntimeWeaponActor& weapon); // Xbox 0x000C7A50, ESI actor
void update_runtime_container_weapon_state(RuntimeWeaponActor& weapon, u16 state); // Xbox 0x000C76A0, vtable +0x44
void enter_runtime_container_weapon_state(RuntimeWeaponActor& weapon, u16 state,
    u16 previous); // Xbox 0x000C7870, vtable +0x3C
void leave_runtime_container_weapon_state(RuntimeWeaponActor& weapon, u16 state, u16 next); // Xbox 0x000C7990, vtable +0x40
}
