#pragma once
#include "gbtg/entity.h"
#include <span>

namespace gbtg
{
struct Game;
u8 runtime_player_damage_enabled(const Actor& actor,
    std::span<const u32, 2> blocked, std::span<const u8> modes, u32 mode); // Xbox 0x00028D50
u32 subtract_runtime_body_damage_health(std::span<u8> body, const Actor& actor,
    const EntityBitBlock& flags, u32 amount, u32 double_damage,
    std::span<const u32, 2> blocked, std::span<const u8> modes, u32 mode); // Xbox 0x00021120
void adjust_runtime_body_damage_kind(std::span<u8> damage,
    std::span<const u8> attributes, const EntityBitBlock& flags); // Xbox 0x000211D0
void set_runtime_body_damage_sources(std::span<u8> body, u32 kind,
    u32 amount, u32 first, u32 second, u32 third, u32 increment); // Xbox 0x00024370
bool notify_runtime_body_death_once(std::span<u8> body,
    u32 actor_address, Game& game); // Xbox 0x00024910
bool runtime_body_death_effect_selected(std::span<const u8> attributes,
    u32& random); // Xbox 0x00024620
void notify_runtime_body_blocked_damage(Game& game, u32 source,
    u32 target, u32 first, u32 second); // Xbox 0x00024A80
void notify_runtime_body_classified_damage(Game& game, u32 source,
    u32 target, u32 first, u32 second); // Xbox 0x00024AD0
void notify_runtime_body_damage(Game& game, u32 source, u32 target,
    u32 first, u32 second, u32 third, u32 fourth, u32 fifth,
    u32 skip_script); // Xbox 0x00024A00
}
