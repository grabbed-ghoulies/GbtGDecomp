#include "powerup_awards.h"
#include "../frontend/app_error_screen.h"
#include "actor_live_attachments.h"
#include "body_request_selection.h"
#include "mind_target_policy.h"
#include "../saves/save_summary.h"
#include "actor_transform.h"
#include "body_message_helpers.h"
#include "trigger_actor.h"
#include "gbtg/actor_spawn_factory.h"
#include <array>
#include <bit>
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
        const char16_t* name = context.profile.name.data(); // DAT_00506C90
        store_runtime_save_challenges(context.profile); // 0x000F3230
        if (save_game_word(name, save_record_field::unk_184.offset) != 0) // 0x0010DC20, EBX the name
        {
            clear_save_game_word_184(name); // 0x0010E2C0
        }
        // 0x000F32F0 on a failed write: apperror gamesystem_corruptgamemessage for the name.
        queue_save_game(name, [](const void* argument)
        {
            show_runtime_corrupt_game(static_cast<const char16_t*>(argument)); // 0x000F32F0
        }, context.saves); // 0x0010DA80, EAX the name
    }
    show_runtime_bonus_counter(0, won, context.counters, context.scoreboard); // 0x0012D7E0, EDI 0
    return won;
}

// Xbox 0x001349E0, ECX fifth, EDX fourth
u32 spawn_runtime_placed_actor(const char* name, const Vec3& position, const Vec3& euler, float scale,
    const char* second_override, const char* fourth_override, const char* fifth_override, u32 tag, u16 word,
    const Vec3& message_position, const Vec3& message_euler, const u8* attributes, u32 last, i16 scene_key,
    RuntimeActorPlacementSpawn& context)
{
    const u32 created = context.keep(create_spawn_actor(context.factory, context.resources, static_cast<const u8*>(context.scene_records),
        name, position, euler, scale, nullptr, nullptr, second_override, fourth_override, fifth_override,
        scene_key)); // 0x001347C0, EAX name, ECX position, EBX euler
    if (created == 0 || tag == 0)
    {
        return created;
    }
    const std::span<u8> bytes = context.bytes_at(created);
    std::memcpy(bytes.data() + 0x990, &word, 2);
    if (attributes != nullptr)
    {
        copy_runtime_trigger_attributes(bytes.data() + 0x998, attributes); // 0x000280D0, EAX +0x998
    }
    const std::array<u32, 9> message{0x4D, tag, std::bit_cast<u32>(message_position.x),
        std::bit_cast<u32>(message_position.y), std::bit_cast<u32>(message_position.z),
        std::bit_cast<u32>(message_euler.x), std::bit_cast<u32>(message_euler.y), std::bit_cast<u32>(message_euler.z),
        last};
    context.message(created, message); // Vtable +0xC.
    return created;
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
        sound = 0;
        if (!context.complete_event)
        {
            throw Error("0x00123E70's powerup kind 5 completes the event sequence (0x000D0870) without it bound");
        }
        context.complete_event(); // 0x000D0870
        break;
    case 6:
        throw Error("0x00123E70's powerup kind 6 reads the DAT_005064E0 challenges and DAT_004E17D8 timers; "
            "their native views are not bound");
    case 7:
        if (!context.marked_actors)
        {
            throw Error("0x00123E70's powerup kind 7 walks DAT_00511B38 without its actors bound");
        }
        // Each listed actor whose +0xCDC is 1 or 5 has its +0x930 (the node's -4) set to the amount.
        for (Actor* actor : context.marked_actors())
        {
            if (actor->mask_mode == 1 || actor->mask_mode == 5)
            {
                actor->animation_pause = amount;
            }
        }
        break;
    case 8:
    case 9:
    case 0xA:
    case 0xB:
    case 0xD:
    case 0x12:
        sound = 0;
        break;
    case 0xC:
    {
        sound = 0;
        if (context.minicooper == nullptr)
        {
            throw Error("0x00123E70's powerup kind 0xC spawns the Mini Cooper (0x001349E0) without the scene's views");
        }
        RuntimeMiniCooperSpawn& minicooper = *context.minicooper;
        const Actor& awarded = minicooper.actor_at(player);
        const Vec3 position = actor_world_position(awarded); // 0x000412A0
        const Vec3 euler = actor_world_euler(awarded); // 0x00041820
        const u32 created = spawn_runtime_placed_actor("aid_objparams_ghoulies_actor_minicooper", position, euler, 1.f,
            nullptr, nullptr, nullptr, 0x1A, 0, position, euler, nullptr, 0, 0, minicooper.spawn); // 0x001349E0
        if (created != 0)
        {
            set_runtime_body_bit(minicooper.body_bits(created), 7); // 0x0001EFD0, EAX its body, ECX 7
            if (minicooper.count < 0 || minicooper.count >= i32(minicooper.actors.size()))
            {
                throw Error("0x00123E70's Mini Cooper list (DAT_00512324) is full");
            }
            minicooper.actors[size_t(minicooper.count)] = created;
            minicooper.times[size_t(minicooper.count)] = amount;
            ++minicooper.count;
            cleared = true;
        }
        break;
    }
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
    {
        sound = 0;
        if (context.active_actors == nullptr)
        {
            throw Error("0x00123E70's powerup kind 0x13 walks DAT_00511B48 without its actors bound");
        }
        RuntimePowerupActors& actors = *context.active_actors;
        const std::array<u32, 1> freeze{1};
        for (u32 address = actors.first(); address != 0; address = actors.next(address))
        {
            RuntimePowerupActor view = actors.actor_at(address);
            if (read_u32(view.bytes, 0x98C) != 0) // Node +0x48.
            {
                const u16 mask = script_actor_current_mask(view.actor, actors.query); // 0x00028360, EAX actor
                if (mask == 3 || mask == 0x24)
                {
                    continue;
                }
            }
            bool frozen = true;
            if (!view.strategy.empty() && script_actor_current_mask(view.actor, actors.query) == 0) // 0x00028360
            {
                if (view.strategy[0x1C] != 0)
                {
                    continue;
                }
                RuntimePowerupActor owner = actors.actor_at(read_u32(view.strategy, 8));
                const u32 group = (read_u16(owner.bytes, 0xCDC) == 2 || read_u32(owner.bytes, 0x93C) != 0) ? 1u : 0u;
                if (actors.blocked[group] != 0 || actors.modes[actors.mode] == 0)
                {
                    continue;
                }
                if (owner.ring != nullptr)
                {
                    const RuntimePlayerGoalRing& ring = *owner.ring;
                    const std::span<const u8> top = ring.tail == ring.capacity ? actors.address_zero
                        : ring.rows.subspan(size_t(ring.head) * ring.stride);
                    if (read_u16(top, 0) != 0)
                    {
                        frozen = false;
                    }
                }
            }
            if (!frozen)
            {
                continue;
            }
            std::memcpy(view.bytes.data() + 0x978, &amount, 4);
            std::span<u8> mind = view.mind;
            if (!mind.empty() && read_u32(mind, 0x10) != 0
                && !runtime_mind_target_is_eligible(mind, view.bytes, view.actor, address,
                    actors.target_at(read_u32(mind, 0x10)), true)) // 0x00029C80, EDX 1
            {
                if (read_u32(mind, 0x10) != 0)
                {
                    const std::array<u32, 2> drop{0x64, 0};
                    actors.message(read_u32(mind, 8), drop); // Mind +8's vtable +0xC.
                }
                const u32 zero = 0;
                std::memcpy(mind.data() + 0x10, &zero, 4);
            }
            actors.message(address, freeze); // Actor vtable +0xC with {1}.
        }
        break;
    }
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
