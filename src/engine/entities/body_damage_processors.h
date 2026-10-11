#pragma once
#include "body_damage_motion.h"
#include "body_damage_notices.h"
#include "body_message_helpers.h"
#include "body_damage_animation.h"

namespace gbtg
{
struct RuntimeBodyDamageProcessor
{
    Actor& actor;
    u32 actor_address;
    EntityBitBlock& flags;
    std::span<const u8> attributes;
    RuntimeBodyAttachedActor* first_source;
    RuntimeBodyAttachedActor* second_source;
    const RuntimeBodyDamageTables& tables;
    std::span<const u8> push_keys;
    std::span<const u8> amount_keys; // DAT_00217DE8.
    u32& player_damage; // DAT_00512200.
    u32 second_source_live; // Source actor +0xCE0.
    std::span<const u32, 2> player_blocked;
    std::span<const u8> player_modes;
    u32& player_mode;
    RuntimeBodyActionMessages& actions;
    std::function<u16()> defensive_action; // Body vtable +0x44.
    std::span<const u16, 3> death_actions; // DAT_00217EB8.
    const RuntimeDamageAnimationRows& animation_rows;
    std::span<char> animation_name;
};
void process_runtime_body_reaction_hit(std::span<u8> body,
    std::span<const u8> damage, std::span<u32, 3> result, u32 push_key,
    u32 phase, RuntimeBodyDamageProcessor& context); // Xbox 0x000215C0
void process_runtime_body_interrupting_hit(std::span<u8> body,
    std::span<const u8> damage, std::span<u32, 4> result,
    RuntimeBodyDamageProcessor& context); // Xbox 0x000219C0
void process_runtime_body_turning_hit(std::span<u8> body,
    std::span<const u8> damage, std::span<u32, 3> result, u32 phase,
    RuntimeBodyDamageProcessor& context); // Xbox 0x00021B70
void process_runtime_body_amount_hit(std::span<u8> body,
    std::span<const u8> damage, std::span<u32, 3> result, u32 phase,
    RuntimeBodyDamageProcessor& context); // Xbox 0x00021C50
bool runtime_body_has_death_action(const RuntimeScriptLookupTable* table,
    std::span<const u16, 3> keys); // Xbox 0x00024680
void process_runtime_body_defensive_hit(std::span<u8> body,
    std::span<const u8> damage, std::span<u32, 3> result, u32 phase,
    RuntimeBodyDamageProcessor& context); // Xbox 0x00021760
void process_runtime_body_action_hit(std::span<u8> body,
    std::span<const u8> damage, std::span<u32, 3> result,
    RuntimeBodyDamageProcessor& context); // Xbox 0x00021A90
void process_runtime_body_health_hit(std::span<u8> body,
    std::span<const u8> damage, std::span<u32, 3> result,
    RuntimeBodyDamageProcessor& context); // Xbox 0x00022010
}

