#include "ghouly_spawn_trigger.h"
#include "actor_transform.h"
#include "entity.h"

#include <bit>

namespace gbtg
{
std::function<std::vector<GhoulySpawnPlayerLink>()> g_spawn_trigger_players;

// Xbox 0x0013E980, EBX query, XMM0 radius
i32 check_ghouly_spawn_trigger(const Vec3& query, float radius, float chance, u32& random_state)
{
    if (!(radius > 0.f)) // DAT_0032B100.
    {
        return 0;
    }
    if (!g_spawn_trigger_players)
    {
        throw Error("0x0013E980 walks DAT_00511B50 outside a native scene");
    }
    const float radius_squared = radius * radius;
    for (const GhoulySpawnPlayerLink& link : g_spawn_trigger_players())
    {
        if (link.disabled_ce0 != 0 && (link.mode_cdc == 2 || link.state_93c != 0) && kRuntimeScareHold != 0)
        {
            continue;
        }
        const Vec3 position = actor_world_position(*link.actor); // 0x000412A0
        const Vec3& previous = link.previous_position;
        const float dx = position.x - query.x;
        const float dy = position.y - query.y;
        const float dz = position.z - query.z;
        const float now = dz * dz + dy * dy + dx * dx;
        const float old_x = query.x - previous.x;
        const float old_y = query.y - previous.y;
        const float old_z = query.z - previous.z;
        const float before = old_z * old_z + old_y * old_y + old_x * old_x;
        if (before > radius_squared && radius_squared >= now)
        {
            random_state = random_state * 0x19660Du + 0x3C6EF35Fu;
            const float random = std::bit_cast<float>((random_state & 0x7FFFFFu) | 0x3F800000u) - 1.f;
            if (chance >= random)
            {
                return 1;
            }
            return -1;
        }
    }
    return 0;
}
}
