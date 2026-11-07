#pragma once
#include "body_damage_fields.h"
#include "body_damage_tables.h"
#include "body_action_selection.h"

namespace gbtg
{
float runtime_actor_bound_angle(std::span<const u8> bound,
    const Actor& origin); // Xbox 0x00041970
u32 runtime_body_target_direction(const Actor& actor,
    std::span<const u8> target, float heading); // Xbox 0x0001CC10
i32 runtime_body_target_turn_direction(const Actor& actor,
    std::span<const u8> target, u32* within_angle); // Xbox 0x00026830
void runtime_horizontal_direction(Vec3& direction, float angle,
    float length); // Xbox 0x001060A0
void offset_runtime_horizontal_point(Vec3& output, const Vec3& source,
    float angle, float length); // Xbox 0x001060F0
float runtime_body_hit_angle(std::span<const u8> hit); // Xbox 0x0001D540
float runtime_actor_angle_toward(const Actor& target, const Actor& origin); // Xbox 0x00041900
float step_runtime_damage_angle(float current, float target, float step); // Xbox 0x00100FE0
void turn_runtime_body_toward_damage(Actor& actor, const Actor& source,
    std::span<const u8> attributes); // Xbox 0x000246D0
void push_runtime_body_from_damage(Actor& actor, u32 actor_address,
    const BodyDamage& damage, std::span<const u8> attributes, u32 key,
    RuntimeBodyAttachedActor* first_source, RuntimeBodyAttachedActor* second_source,
    const RuntimeBodyDamageTables& tables, std::span<const u8> keys); // Xbox 0x00022400
void set_runtime_body_damage_motion(std::span<u8> body, const Vec3& direction,
    float first, float second, float third, float fourth); // Xbox 0x00024830
}
