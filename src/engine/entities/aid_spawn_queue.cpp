#include "aid_spawn_queue.h"

#include <bit>

namespace gbtg
{
// Xbox 0x000F4F80
bool tick_aid_spawn_queue(AidSpawnQueueRecord& record, const Vec3& position, float delta,
    AidSpawnQueueContext& context)
{
    if (record.intervals == nullptr || record.names == nullptr || record.controllers == nullptr)
    {
        return false;
    }
    if (record.current == nullptr)
    {
        record.current = reinterpret_cast<const char*>(select_spawn_name(*record.names,
            context.spawn.random_state));
    }
    if (record.current == nullptr)
    {
        return true;
    }
    float timer = std::bit_cast<float>(record.intervals->previous);
    const bool ready = tick_spawn_queue_interval(*record.intervals, timer, delta, record.current,
        context.registry, context.error, context.spawn);
    record.intervals->previous = std::bit_cast<i32>(timer);
    if (ready)
    {
        record.attempts.fill(0);
        const auto& pool = *record.controllers->pool;
        i32 iterations = i32(pool.cursor - pool.begin) / pool.stride;
        if (iterations < 21)
        {
            iterations = 20;
        }
        u32 key = 0;
        if (iterations != 0)
        {
            do
            {
                key = select_spawn_controller(*record.controllers, position,
                    context.spawn.random_state, context.actors);
                if (key == 0)
                {
                    break;
                }
                ++record.attempts[key];
                const char* attributes;
                if (record.attributes == nullptr)
                {
                    attributes = nullptr;
                }
                else
                {
                    attributes = reinterpret_cast<const char*>(select_spawn_attributes(
                        *record.attributes, context.spawn.random_state));
                }
                if (request_ghoulybox(record.current, key, attributes) != 0)
                {
                    if (record.remaining > 0)
                    {
                        --record.remaining;
                    }
                    break;
                }
                --iterations;
            }
            while (iterations != 0);
        }
        if (iterations == 0 || key == 0)
        {
            if (record.remaining == -2 &&
                u32(count_spawn_controller_attempts(record.attempts)) == record.controllers->distinct)
            {
                return true;
            }
        }
        record.current = nullptr;
    }
    if (record.remaining != -3)
    {
        return record.remaining == 0;
    }
    if (context.game_state == 3 && context.game_gate == 0)
    {
        return false;
    }
    return true;
}
}
