#include "powerup_flags.h"
#include "gbtg/log.h"

namespace gbtg
{
const u32* g_runtime_weapon_boost = nullptr;

u32 runtime_weapon_boost_active()
{
    if (g_runtime_weapon_boost == nullptr)
    {
        throw Error("DAT_00512278 (the PermanentWeaponBoost powerup row) is read outside a native scene");
    }
    return *g_runtime_weapon_boost;
}
}
