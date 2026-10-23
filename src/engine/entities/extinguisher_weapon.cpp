#include "extinguisher_weapon.h"
#include "weapon_actor_detail.h"
#include "gbtg/anim.h"
#include "gbtg/resource_error.h"
#include <bit>
#include <cstring>
#include <unordered_map>

namespace gbtg
{
using namespace weapon_detail;

namespace
{
namespace field = extinguisher_weapon_field;
namespace parameter = extinguisher_weapon_parameter;
namespace gun = gun_weapon_field;

// The spray AID's bytes (+0xEFC), by the weapon's bytes.
std::unordered_map<const u8*, std::span<const u8>> extinguisher_sprays;

u32 trail(std::span<const u8> bytes, u32 index)
{
    return read_u32(bytes, field::kTrails + index * 4);
}
}

// Xbox 0x000C7B50
void extinguisher_weapon_object_sizes(ObjectSizes& sizes)
{
    sizes.instance = field::kSize;
    sizes.parameters = parameter::kSize;
}

// Xbox 0x000C7BE0
bool initialize_runtime_extinguisher_weapon(RuntimeWeaponActor& weapon, std::span<const u8> parameters)
{
    auto bytes = weapon.bytes;
    const char* name = field_data(parameters, parameter::spray);
    std::span<const u8> spray{};
    if (read_u32(parameters, parameter::spray.offset) == kAidPrefix)
    {
        spray = acquire_animation_resource(weapon.registry, name); // 0x00035E90
        if (spray.data() == nullptr)
        {
            resource_error(weapon.error, 2); // 0x00032700
        }
    }
    extinguisher_sprays[bytes.data()] = spray;
    set(bytes, gun::recharge, 0.075f); // DAT_0032B44C.
    set(bytes, gun::drain, 0.15f); // DAT_0032B168.
    set(bytes, field::spray, spray.data() != nullptr ? 1u : 0u);
    set(bytes, field::unk_F28, 0.01f); // DAT_0032B124.
    return true;
}

// Xbox 0x000C7C60
void destroy_runtime_extinguisher_weapon(RuntimeWeaponActor& weapon)
{
    auto bytes = weapon.bytes;
    if (get(bytes, field::spray) != 0)
    {
        release_animation_resource(weapon.registry, extinguisher_sprays[bytes.data()]); // 0x00035C00
        set(bytes, field::spray, 0u);
    }
    extinguisher_sprays.erase(bytes.data());
    if (get(bytes, field::active) != 0)
    {
        throw Error("0x000C7C60 frees an extinguisher spray block that the untranslated 0x000C7D10 made");
    }
    for (u32 index = 0; index < 8; ++index)
    {
        if (trail(bytes, index) != 0)
        {
            throw Error("0x000C7C60 frees an extinguisher trail block that the untranslated 0x000C8D30 made");
        }
    }
}

// Xbox 0x000C7D60
void update_runtime_extinguisher_spray(RuntimeWeaponActor& weapon, float)
{
    auto bytes = weapon.bytes;
    set(bytes, gun::drain, std::bit_cast<float>(get(bytes, gun::unk_EE8)) * 0.135f + 0.015f); // DAT_0032B384, DAT_0032B380.
    if (get(bytes, field::active) != 0)
    {
        throw Error("untranslated Xbox 0x000C7E40 (the extinguisher's spray block update)");
    }
    for (u32 index = 0; index < 8; ++index)
    {
        if (trail(bytes, index) != 0)
        {
            throw Error("untranslated Xbox 0x000C7E40 (the extinguisher's trail block update)");
        }
    }
}

// Xbox 0x000C8C70
void draw_runtime_extinguisher_spray(RuntimeWeaponActor& weapon)
{
    auto bytes = weapon.bytes;
    if (get(bytes, field::active) != 0)
    {
        throw Error("untranslated Xbox 0x000C8CB0 (the extinguisher's spray block draw)");
    }
    for (u32 index = 0; index < 8; ++index)
    {
        if (trail(bytes, index) != 0)
        {
            throw Error("untranslated Xbox 0x000C8CB0 (the extinguisher's trail block draw)");
        }
    }
}
}
