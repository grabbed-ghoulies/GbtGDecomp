#include "ghouly_spawn_trigger.h"
#include "actor_transform.h"
#include "entity.h"

#include <bit>

namespace gbtg
{
// Xbox 0x0013E980
i32 check_ghouly_spawn_trigger(const Vec3& query, float radius, float chance,
    i32 blocked_players, u32& random_state)
{
    if (radius > 0.f)
    {
        const float radius_squared = radius * radius;
        auto* link = static_cast<GhoulySpawnPlayerLink*>(entity_detail::g_world.slot.next);
        for (; link != nullptr; link = link->next)
        {
            if (link->disabled_ce0 == 0 ||
                ((link->mode_cdc != 2 && link->state_93c == nullptr) || blocked_players == 0))
            {
                const Vec3 position = actor_world_position(*link->actor);
                const Vec3& previous = *link->previous_position;
                const float old_x = query.x - previous.x;
                const float old_y = query.y - previous.y;
                const float old_z = query.z - previous.z;
                const float dx = position.x - query.x;
                const float dy = position.y - query.y;
                const float dz = position.z - query.z;
                if (old_z * old_z + old_y * old_y + old_x * old_x > radius_squared &&
                    radius_squared >= dz * dz + dy * dy + dx * dx)
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
        }
    }
    return 0;
}
}
