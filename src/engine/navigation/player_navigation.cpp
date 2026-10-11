#include "player_navigation.h"
#include "path_routes.h"
#include "../entities/actor_transform.h"
#include "../entities/body_shape_push.h"

namespace gbtg
{
// Xbox 0x00136EB0
u32 request_runtime_player_navigation_path(RuntimePlayerNavigation& context,
    const Vec3& target, i32 target_floor, Vec3& direction, u32& changed_floor)
{
    using namespace entity_detail;
    NavigationPathSearch* const search = context.search;
    if (search == nullptr)
    {
        return 0;
    }
    const Vec3 position = actor_world_position(context.actor);
    const i32 floor = context.floor;
    const i32 start_cell = navigation_cell_index(floor, position);
    i32 finish_cell = navigation_cell_index(floor, target);
    if (start_cell == -1)
    {
        return 0;
    }
    if (finish_cell == -1)
    {
        return 0;
    }
    const Vec3* destination = &target;
    Vec3 converted;
    if (target_floor != floor)
    {
        if (navigation_floor_connection_point(floor, target_floor, target, converted) == 0)
        {
            return 0;
        }
        destination = &converted;
        finish_cell = navigation_cell_index(target_floor, converted);
        changed_floor = 1;
    }
    const u32 retargeted = retarget_navigation_search_route(*search, start_cell, finish_cell);
    auto* route = static_cast<GhoulySpawnPool*>(search->scratch);
    if (route == nullptr || retargeted == 0 || navigation_path_search_route_is_open(*search) == 0)
    {
        NavigationCellRegistration* const registration = context.registration;
        if (registration != nullptr)
        {
            decrement_navigation_cell_counts(*registration, u32(context.floor));
        }
        if (navigation_cell_has_no_open_neighbor(*search->grid, u32(start_cell), 0))
        {
            Vec3 nearest;
            const i32 found = navigation_nearest_open_cell(u32(start_cell), context.floor, &nearest);
            if (found != -1)
            {
                direction = Vec3{nearest.x - position.x, nearest.y - position.y, nearest.z - position.z};
                if (registration != nullptr)
                {
                    increment_navigation_cell_counts(*registration, u32(context.floor));
                }
                return 1;
            }
            if (registration == nullptr)
            {
                return 0;
            }
            increment_navigation_cell_counts(*registration, u32(context.floor));
            return 0;
        }
        begin_navigation_path_search(*search, start_cell, finish_cell);
        const float elapsed = runtime_actor_step(context.actor, context.clock);
        update_navigation_path_search(*search, read_u32(context.body, 0x878), elapsed);
        if (registration != nullptr)
        {
            increment_navigation_cell_counts(*registration, u32(context.floor));
        }
        if (navigation_path_search_has_result(*search) == 0)
        {
            return 0;
        }
        route = take_navigation_search_route(*search, position, *destination);
    }
    else
    {
        set_navigation_route_endpoints(*route, position, *destination);
    }
    if (route == nullptr)
    {
        return 0;
    }
    Vec3 sampled;
    navigation_route_direction(*route, direction, &sampled, float(search->grid->cell) * 0.5f);
    scale_runtime_collision_vector(direction, float(navigation_route_length(*route)));
    return 1;
}
}
