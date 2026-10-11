#pragma once
#include "body_damage_notices.h"
#include "entity_types.h"

namespace gbtg
{
struct RuntimeBodyDamageTables
{
    std::span<const u8> values; // DAT_00433C9C, stride 0xC.
    std::span<const u8> classes; // DAT_00545AC4, stride 0x87.
};
bool runtime_body_contact_damage_blocked(const EntityBitBlock& flags,
    const entity_detail::SegmentPool* contacts, const Actor& contact_actor,
    float contact_y); // Xbox 0x00022870
float runtime_body_damage_direction(float facing, const Vec3& direction); // Xbox 0x000244E0
float runtime_body_damage_push_amount(const Actor& actor, u16 actor_class,
    u32 key, const RuntimeBodyDamageTables& tables, std::span<const u8> keys); // Xbox 0x000239C0
bool runtime_body_damage_parameters(const Actor& actor, u16 actor_class,
    u32 key, const RuntimeBodyDamageTables& tables, std::span<const u8> keys,
    float& first, float& second, float& third, float& fourth); // Xbox 0x00023A50
float interpolate_runtime_damage_value(u8 key, u8 value,
    const RuntimeBodyDamageTables& tables); // Xbox 0x001339C0
float runtime_class_damage_value(u8 key, u16 actor_class,
    const RuntimeBodyDamageTables& tables); // Xbox 0x00133BD0
i32 runtime_actor_damage_value(u8 key, u16 actor_class,
    const RuntimeBodyDamageTables& tables); // Xbox 0x00028DA0
bool runtime_actor_is_player(const Actor& actor); // Xbox 0x00028D00
bool runtime_actor_is_live_player(const Actor& actor, u32 live); // Xbox 0x00028D20
i32 runtime_body_damage_pulse_count(const Actor& actor, u16 actor_class,
    const RuntimeBodyDamageTables& tables); // Xbox 0x00023CB0
float runtime_body_damage_pause(const Actor& actor, u16 actor_class,
    const RuntimeBodyDamageTables& tables); // Xbox 0x00023E20
float runtime_body_damage_source_pause(const Actor& actor, u16 actor_class,
    const RuntimeBodyDamageTables& tables); // Xbox 0x00023F70
void reduce_runtime_body_damage_pulse(std::span<u8> body,
    const Actor& actor, i32 count); // Xbox 0x0001FF60
u32 apply_runtime_body_damage_health(std::span<u8> body, const Actor& actor,
    const Actor* source, u32 source_live, u32 player_damage,
    const EntityBitBlock& flags, u8 key, u16 actor_class,
    const RuntimeBodyDamageTables& tables, std::span<const u8> keys,
    std::span<const u32, 2> blocked, std::span<const u8> modes, u32 mode); // Xbox 0x00022310
u32 runtime_body_damage_amount(std::span<const u8> body, const Actor& actor,
    const Actor* source, u32 source_live, u32 player_damage, u8 key, u16 actor_class,
    const RuntimeBodyDamageTables& tables, std::span<const u8> keys); // Xbox 0x000238E0
}
