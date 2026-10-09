#include "live_spawn_tick.h"
#include "../entities/actor_transform.h"

namespace gbtg
{
// Xbox 0x0011B670
void tick_live_spawn_queues(LiveSpawnTickContext& context)
{
    if ((context.queue.spawn.flags & 0x40) == 0)
    {
        const float delta = context.pause > 0.f ? 0.f : context.frame_delta * context.time_scale;
        Vec3 position;
        if (context.player_address == 0 || context.player_address == 0x93C)
        {
            position = Vec3{};
        }
        else
        {
            position = actor_world_position(*context.actors.at(context.player_address - 0x93C));
        }
        auto* aid = g_aid_spawn_queue.next;
        while (aid != nullptr)
        {
            auto* next = aid->next;
            if (tick_aid_spawn_queue(*aid, position, delta, context.queue))
            {
                release_aid_spawn_queue(aid);
            }
            aid = next;
        }
        auto* record = g_side_list.next;
        while (record != nullptr)
        {
            auto* owner = static_cast<GhoulySpawnRecord*>(record);
            record = record->next;
            if (tick_ghouly_spawn_record(*owner, delta, context.queue.spawn) && owner->prev != nullptr)
            {
                owner->prev->next = owner->next;
                if (owner->next != nullptr)
                {
                    owner->next->prev = owner->prev;
                }
                owner->prev = nullptr;
                owner->next = nullptr;
            }
        }
    }
}
}
