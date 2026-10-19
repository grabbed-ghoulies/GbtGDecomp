#include "powerup_awards.h"
#include "actor_live_attachments.h"
#include "body_request_selection.h"
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

// Xbox 0x000F24C0
i32 award_runtime_challenge(u32 index, RuntimeChallengeAward& context)
{
    auto& challenges = context.profile.challenges;
    challenges.bytes[index >> 3] |= u8(1u << (index & 7));
    const i32 won = count_runtime_request_bits({u32(challenges.kind), challenges.bytes}); // 0x00066180, EDX the block
    const char* notice = nullptr;
    if (won == 1)
    {
        notice = "challenge_firstbonusbook"; // 0x00325F04
    }
    else if (won == 5)
    {
        notice = "challenge_firstchallenge"; // 0x00325EE8
    }
    if (notice != nullptr && runtime_intro_notice_ready(context.notices) != 0) // 0x0013FF70
    {
        std::strcpy(context.notices.intro_name.data(), notice);
        context.notices.intro_pending = 1;
    }
    if (context.profile.name[0] != 0)
    {
        throw Error("untranslated 0x000F3230 (the challenge save of DAT_00506C90) from 0x000F24C0");
    }
    show_runtime_bonus_counter(0, won, context.counters, context.scoreboard); // 0x0012D7E0, EDI 0
    return won;
}

// Xbox 0x00123E70
void award_runtime_powerup(i32 id, u32 player, u32 kind, float amount, u32 extra,
    RuntimePowerupAwardContext& context)
{
    auto& row = context.table.rows[kind];
    const u32 was_active = row.active;
    i32 sound = -1;
    u32 notice = was_active;
    row.active = 1;
    row.amount = amount + row.amount;
    row.extra = extra;
    row.player = player;
    row.id = id;
    if (kind != 0)
    {
        show_runtime_powerup_caption(kind, context.caption); // 0x00132540, EAX kind
        if (was_active == 0 && row.message == 0)
        {
            if (context.table.board == nullptr && kind != 1 && kind != 3 && kind != 5 && kind != 6)
            {
                throw Error("0x00132450 adds a powerup message to DAT_00545ABC before its board exists");
            }
            row.message = add_runtime_powerup_message(kind, *context.table.board, context.scoreboard); // 0x00132450
        }
    }
    bool cleared = false;
    switch (kind)
    {
    case 1:
    {
        notice = 1;
        i32 won = 0;
        if (-1 < row.id)
        {
            won = award_runtime_challenge(u32(row.id), context.challenge); // 0x000F24C0, ECX the id
        }
        sound = won % 5 != 0 ? 2 : 3;
        break;
    }
    case 2:
        sound = 0;
        // The walk of DAT_00511B38 to its last node changes nothing.
        break;
    case 3:
        cleared = true;
        sound = 0;
        break;
    case 4:
    case 0x10:
    case 0x11:
        sound = 1;
        break;
    case 5:
        cleared = true;
        throw Error("untranslated 0x000D0870 from 0x00123E70 (powerup kind 5)");
    case 6:
        throw Error("0x00123E70's powerup kind 6 reads the DAT_005064E0 challenges and DAT_004E17D8 timers; "
            "their native views are not bound");
    case 7:
        throw Error("0x00123E70's powerup kind 7 writes DAT_00511B38's actors; their native views are not bound");
    case 8:
    case 9:
    case 0xA:
    case 0xB:
    case 0xD:
    case 0x12:
        sound = 0;
        break;
    case 0xC:
        throw Error("untranslated 0x001349E0 (the minicooper spawn) from 0x00123E70 (powerup kind 0xC)");
    case 0xE:
        sound = 1;
        if (context.table.rows[0xF].active != 0) // DAT_005122A8
        {
            cancel_runtime_powerup(0xF, context.table); // 0x00124320, EAX 0xF
        }
        break;
    case 0xF:
        sound = 0;
        if (context.table.rows[0xE].active != 0) // DAT_00512290
        {
            cancel_runtime_powerup(0xE, context.table);
        }
        break;
    case 0x13:
        throw Error("0x00123E70's powerup kind 0x13 walks DAT_00511B48's actors (0x00028360, 0x00029C80); "
            "their native views are not bound");
    default:
        break;
    }
    restart_audio_stream(reinterpret_cast<const char*>(context.sounds.data() + size_t(sound + 1) * 0x80), 0x12,
        context.streams); // 0x00038880, EBX 0x00312250 + the sound * 0x80
    if (player != 0)
    {
        const i32 message[5]{0x86, i32(player), i32(kind), i32(extra), i32(notice)};
        broadcast_actor_pulse_message(message); // 0x00122C70, EDI the message
    }
    if (cleared)
    {
        row.active = 0;
        row.player = 0;
        row.amount = 0.f;
        row.extra = 0;
        row.id = 0;
    }
}
}
