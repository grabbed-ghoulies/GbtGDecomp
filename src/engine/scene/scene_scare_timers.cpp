#include "scene_scare_timers.h"

namespace gbtg
{
// Xbox 0x00123E00
void remove_runtime_scared_actor(u32 actor, RuntimeSceneScareTimers& context)
{
    i32 index = 0;
    if (context.count > 0)
    {
        while (context.actors[index] != actor)
        {
            ++index;
            if (index >= context.count)
            {
                return;
            }
        }
        context.actors[index] = context.actors[context.count - 1];
        context.times[index] = context.times[context.count - 1];
        --context.count;
    }
}

// Xbox 0x00123BA0
void tick_runtime_scene_scare_timers(RuntimeSceneScareTimers& context)
{
    // This pass reads dt * scale directly; the freeze word is not tested.
    for (u32 index = 0; index < 20; ++index)
    {
        auto& row = context.powerups.rows[index];
        if (row.active != 0 && row.amount > 0.0f)
        {
            if (context.clock.block_decay == 0 &&
                context.clock.decay_enabled[context.clock.decay_index] != 0 && row.amount > 0.0f)
            {
                row.amount -= context.clock.dt * context.clock.scale;
                // COMISS zero, remaining / JC retains unordered values.
                if (0.0f >= row.amount)
                {
                    row.amount = 0.0f;
                    cancel_runtime_powerup(index, context.powerups);
                    continue;
                }
            }
            if (row.message != 0)
            {
                set_runtime_scoreboard_message_warning(row.message, 3.0f > row.amount,
                    *context.powerups.board);
            }
        }
    }
    float maximum = 0.0f;
    i32 index = 0;
    if (context.count > 0)
    {
        do
        {
            if (context.times[index] > maximum)
            {
                maximum = context.times[index];
            }
            if (context.clock.block_decay == 0 &&
                context.clock.decay_enabled[context.clock.decay_index] != 0 &&
                context.times[index] > 0.0f)
            {
                context.times[index] -= context.clock.dt * context.clock.scale;
                if (0.0f >= context.times[index])
                {
                    context.times[index] = 0.0f;
                    const u32 message[9]{0x4D, 0x1B, 0, 0, 0, 0, 0, 0, 0};
                    context.message(context.actors[index], message);
                    context.actors[index] = context.actors[context.count - 1];
                    context.times[index] = context.times[context.count - 1];
                    --context.count;
                    if (context.count == 0)
                    {
                        cancel_runtime_powerup(0xC, context.powerups);
                    }
                }
                else
                {
                    ++index;
                }
            }
            else
            {
                ++index;
            }
        }
        while (index < context.count);
    }
    if (context.count != 0)
    {
        const u32 id = context.powerups.rows[0xC].message; // DAT_00512274.
        set_runtime_scoreboard_message_warning(id, 3.0f > maximum, *context.powerups.board);
    }
}
}
