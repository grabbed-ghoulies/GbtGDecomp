#pragma once
#include "footstep_names.h"
#include "../entities/body_damage_tables.h"
#include "gbtg/sound_runtime.h"

namespace gbtg
{
const char* runtime_actor_tag_suffix(u16 tag,
    std::span<const char* const> names); // Xbox 0x00107780
float runtime_body_step_speed(std::span<const u8> body, const Actor& actor,
    const RuntimeBodyDamageTables& tables); // Xbox 0x000259C0
void format_runtime_body_step_sound(char* output, std::span<const u8> body,
    u32 alternate, std::span<const char* const> names,
    const FootstepSoundNames& sounds); // Xbox 0x0001A020
void format_runtime_body_impact_sound(char* output, std::span<const u8> body,
    const char* kind, std::span<const char* const> names,
    const FootstepSoundNames& sounds); // Xbox 0x0001A0A0
struct RuntimeBodySoundCommandContext
{
    Actor& actor;
    Actor& motion_actor;
    std::span<const u8> motion_body;
    const RuntimeBodyDamageTables& tables;
    std::span<const char* const> names;
    const FootstepSoundNames& sounds;
    RuntimeActorSound& sound; // Actual actor +0x6CC slot.
    RuntimeActorSoundQueues& queues;
};
void command_runtime_body_step_sound(std::span<const u8> body,
    std::span<const u8> command, RuntimeBodySoundCommandContext& context); // Xbox 0x00019B40
void command_runtime_body_floor_sound(std::span<const u8> body,
    RuntimeBodySoundCommandContext& context); // Xbox 0x00019F80
void command_runtime_body_impact_sound(std::span<const u8> body,
    RuntimeBodySoundCommandContext& context); // Xbox 0x00019FD0
}
