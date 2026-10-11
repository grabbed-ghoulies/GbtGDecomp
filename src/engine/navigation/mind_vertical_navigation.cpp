#include "player_navigation.h"
#include "../entities/actor_transform.h"

namespace gbtg
{
// Xbox 0x00136D60
u32 drive_runtime_mind_vertical_navigation(RuntimePlayerNavigationControl& context,
    const Vec3& target, i32 target_floor, float height, float tolerance,
    float radius, float approach, float speed, float vertical_speed,
    u32 keep_attachment, u32& failed)
{
    const u32 result = drive_runtime_player_navigation(context, target, target_floor,
        radius, approach, speed, keep_attachment, failed);
    const auto body = context.navigation.body;
    if (read_u16(body, 0x30) == 0 && read_f32(body, 0x34) == 0.f)
    {
        const Vec3 position = actor_world_position(context.navigation.actor);
        const i32 floor = context.navigation.floor;
        const i32 cell = entity_detail::navigation_cell_index(floor, position);
        if (cell != -1)
        {
            const float current = read_f32(body, 0x1C);
            height = float(entity_detail::g_nav_grids[floor]->cells[size_t(cell)].bottom) + height;
            const float minimum = height - tolerance;
            float vertical = 0.f;
            if (minimum > current)
            {
                vertical = vertical_speed;
            }
            else if (current > height + tolerance)
            {
                vertical = 0.f - vertical_speed;
            }
            set_runtime_input_vertical_axes(context.writable, {0.f, vertical}, context.keyboard);
            if (result == 1)
            {
                if (0.f > vertical && (context.flags.bytes[5] & 2) != 0)
                {
                    return 1;
                }
                // Retail JC rejects unordered minimum/current as well as current < minimum.
                if (!(current >= minimum) || !(height + tolerance >= current))
                {
                    return 0;
                }
                return 1;
            }
        }
    }
    return result;
}
}
