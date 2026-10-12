#include "powerup_awards.h"
#include "actor_live_attachments.h"
#include <cstring>

namespace gbtg
{
// Xbox 0x00124320
void cancel_runtime_powerup(u32 kind, RuntimePowerupTable& table)
{
    auto& row = table.rows[kind];
    const i32 message[3]{0x87, i32(row.player), i32(kind)};
    row.active = 0;
    broadcast_actor_pulse_message(message);
    if (table.board != nullptr)
    {
        remove_runtime_scoreboard_message(row.message, *table.board, table.registry);
    }
    row.message = 0;
    row.active = 0;
    row.player = 0;
    row.extra = 0;
    row.id = 0;
    row.amount = 0.f;
}

// Xbox 0x0004A370
void extend_runtime_scene_timers(float amount, GhoulySpawnPool* timers)
{
    if (timers == nullptr)
    {
        return;
    }
    for (u8* entry = timers->begin; entry < timers->cursor; entry += 0x14)
    {
        u32 kind;
        std::memcpy(&kind, entry, 4);
        if (kind == 0x10 || kind == 0x11)
        {
            float time;
            std::memcpy(&time, entry + 8, 4);
            time = amount + time;
            std::memcpy(entry + 8, &time, 4);
        }
    }
}

// Xbox 0x000D0650
u8* find_runtime_challenge_record(u32 kind, GhoulySpawnPool* challenges)
{
    if (challenges == nullptr)
    {
        return nullptr;
    }
    for (u8* record = challenges->begin; record < challenges->cursor; record += 0x88)
    {
        u32 value;
        std::memcpy(&value, record, 4);
        if (value == kind)
        {
            return record;
        }
    }
    return nullptr;
}
}
