#pragma once
#include "gbtg/types.h"

namespace gbtg
{
// DAT_00512278: the powerup table's PermanentWeaponBoost row (DAT_00512140 kind 0xD, the "Super Weapons" soup) +0
// active, which 0x00123E70 sets through its indexed row store and 0x00124320 clears. The native scene binds it to its
// powerup rows; unset outside one.
extern const u32* g_runtime_weapon_boost;
u32 runtime_weapon_boost_active(); // DAT_00512278
}
