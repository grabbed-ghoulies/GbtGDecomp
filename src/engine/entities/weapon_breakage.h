#pragma once

#include "weapon_actor.h"

namespace gbtg
{
i32 runtime_weapon_stat_row(const RuntimeWeaponStatTables& tables, const char* tag_name); // Xbox 0x00148640, EAX tag
// The tag's row points, with its caption from the loctext table into text; -1 without a row or caption.
i32 runtime_weapon_stat_points(RuntimeWeaponActor& weapon, std::span<char16_t> text); // Xbox 0x00148740
void award_runtime_weapon_points(RuntimeWeaponActor& weapon, u16 category); // Xbox 0x001456D0, ESI actor
void set_runtime_weapon_piece_pairs(RuntimeWeaponActor& weapon, float fraction); // Xbox 0x001457A0, ESI actor
// Launches every remaining piece group (0x0003F820), drops the weapon's hand pieces and scores the break; true
// when a group was launched.
bool break_runtime_weapon(RuntimeWeaponActor& weapon); // Xbox 0x001454F0, EAX actor
bool request_runtime_weapon_variant(RuntimeWeaponActor& weapon, const char* name,
    const char* attributes); // Xbox 0x00148430, EAX name, ECX actor
void scare_near_runtime_weapon(RuntimeWeaponActor& weapon, float radius); // Xbox 0x0013B660, ESI actor
}
