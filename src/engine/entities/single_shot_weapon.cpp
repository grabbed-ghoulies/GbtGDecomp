#include "single_shot_weapon.h"
#include "weapon_actor_detail.h"
#include "actor_attachment_events.h"
#include "actor_template_creation.h"
#include "actor_transform.h"
#include "entity.h"
#include "entity_collision.h"
#include "../camera/entity_camera.h"
#include "gbtg/actor_runtime.h"
#include <array>
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
SingleShotWeaponState& fields_of(std::span<u8> bytes)
{
    return actor_record_view<SingleShotWeaponState>(bytes);
}

const SingleShotWeaponState& fields_of(std::span<const u8> bytes)
{
    return actor_record_view<SingleShotWeaponState>(bytes);
}

}

// Xbox 0x000C8DE0
void single_shot_weapon_object_sizes(ObjectSizes& sizes)
{
    sizes.instance = sizeof(SingleShotWeaponState);
    sizes.parameters = single_shot_weapon_parameter::kSize;
}

// Xbox 0x000C8E70
bool initialize_runtime_single_shot_weapon(RuntimeWeaponActor& weapon, std::span<const u8> parameters)
{
    const char* name = reinterpret_cast<const char*>(field_data(parameters, single_shot_weapon_parameter::projectile));
    std::memcpy(fields_of(weapon.bytes).projectile.data(), name, std::strlen(name) + 1);
    actor_record_view<GunWeaponState>(weapon.bytes).recharge = 0.075f; // DAT_0032B44C.
    actor_record_view<GunWeaponState>(weapon.bytes).drain = 0.f;
    return true;
}

// Xbox 0x000C8EC0
void dispatch_runtime_single_shot_message(RuntimeWeaponActor& weapon, std::span<u32> message)
{
    if (message[0] == 0x9A)
    {
        message[1] = 1;
        return;
    }
    dispatch_runtime_gun_weapon_message(weapon, message); // The tail jump to 0x000CAB40.
}

// Xbox 0x000C8EF0
void fire_runtime_single_shot(RuntimeWeaponActor& weapon, const Vec3& point, const Vec3& direction, const Vec3& aim)
{
    // 0x00145FD0 inlined: the holder whose link actor +0x918 names, while that link is set.
    const ActorLink* link = weapon.actor.link;
    Actor* holder = link != nullptr && link->head != nullptr ? link->owner : nullptr;
    if (holder == nullptr)
    {
        throw Error("0x000C8EF0 places the shot from the holder's position with no holder");
    }
    float yaw = 0.f;
    float pitch = 0.f;
    direction_to_yaw_pitch(direction.x, direction.y, direction.z, yaw, pitch); // 0x00105E20, ECX yaw, stack pitch
    const Vec3 euler{pitch, yaw, 0.f};
    Vec3 start = point;
    Vec3 from = actor_world_position(*holder); // 0x000412A0, ESI holder
    from.y = point.y;
    entity_record_sweep_probe(start, from, 4, 0xC4000); // 0x00122480, EBX the start
    auto& objects = weapon.services.objects;
    const char* name = reinterpret_cast<const char*>(fields_of(weapon.bytes).projectile.data());
    FactoryObject* created = weapon.services.keep(create_runtime_actor_at(name, &start, &euler, 1.f,
        objects.resources, objects.factory, actor_transform_origin, weapon.services.default_euler)); // 0x00112C50
    if (created == nullptr || created->bytes == nullptr)
    {
        return;
    }
    Vec3 toward{aim.x - point.x, aim.y - point.y, aim.z - point.z};
    const float length = toward.z * toward.z + toward.y * toward.y + toward.x * toward.x;
    if (!(length == 0.f)) // UCOMISS; JNP skips a zero length.
    {
        const long double scale = engine_reciprocal_sqrt(length); // 0x001534C0
        toward.x = float(toward.x * scale);
        toward.y = float(toward.y * scale);
        toward.z = float(toward.z * scale);
    }
    const float facing = direction.z * toward.z + direction.y * toward.y + direction.x * toward.x;
    if (0.f > facing)
    {
        toward = direction;
    }
    const u32 projectile = weapon.object_address(*created);
    const u32 holder_address = weapon.services.actor_address(*holder);
    weapon_base(weapon.bytes_at(projectile)).spawn_owner = holder_address;
    weapon.actor_at(projectile)->spawn_owner = holder; // The native view of +0xD8C.
    std::array<u32, 0x42> launch{};
    launch[0] = 0xA5;
    launch[1] = std::bit_cast<u32>(toward.x * 30.f + point.x); // DAT_0032B108.
    launch[2] = std::bit_cast<u32>(toward.y * 30.f + point.y);
    launch[3] = std::bit_cast<u32>(toward.z * 30.f + point.z);
    weapon.send(projectile, launch); // The projectile's vtable +0xC.
    direction_to_yaw_pitch(direction.x, direction.y, direction.z, yaw, pitch); // 0x00105E20
    g_actor_creation_options.position = start; // DAT_004E1C5C.
    g_actor_creation_options.euler = Vec3{pitch, yaw, 0.f};
    g_actor_creation_options.flags = 5;
    dispatch_actor_attachment_records(weapon.view.startup.resource.resource.data(), 0x2B, nullptr,
        &g_actor_creation_options, weapon.base().callouts, 0); // 0x0003BE20 over actor +0x8C
    const float charge = actor_record_view<GunWeaponState>(weapon.bytes).charge - 0.15f; // DAT_0032B168.
    actor_record_view<GunWeaponState>(weapon.bytes).charge = charge;
    if (0.f > charge)
    {
        exhaust_runtime_gun_weapon(weapon, point, direction); // 0x000CAE00
    }
    std::array<u32, 0x42> fired{};
    fired[0] = 0x3E;
    weapon.send(holder_address, fired); // The holder's vtable +0xC.
}
}
