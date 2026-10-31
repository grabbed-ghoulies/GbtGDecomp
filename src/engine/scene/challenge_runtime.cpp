#include "challenge_runtime.h"
#include "../frontend/app_error_screen.h"
#include "../audio/bank_streams.h"
#include "../frontend/dialogue_activation.h"
#include "../frontend/dialogue_measurement.h"
#include "../frontend/dialogue_text_pool.h"
#include "../frontend/dialogue_trophies.h"
#include "../frontend/dialogue_widget_construction.h"
#include "../frontend/dialogue_widget_state.h"
#include "../frontend/dialogue_widget_update.h"
#include "../frontend/gameselect_challenges.h"
#include "../frontend/hud_layout.h"
#include "../frontend/input_activation.h"
#include "../frontend/scoreboard_hud.h"
#include "../frontend/scoreboard_messages.h"
#include "../entities/strategy_target_selection.h"
#include "../saves/runtime_profile.h"
#include "../saves/save_games.h"
#include "../scripts/script_actor_query.h"
#include "../scripts/script_conditions.h"
#include "../scripts/script_event_tallies.h"
#include "../../gbtg/text/localtext_resources.h"

#include <bit>
#include <cstring>
#include <string>

namespace gbtg
{
RuntimeChallengeRun g_challenge_run;
ChallengeRuntimeServices* g_challenge_runtime = nullptr;

ChallengeRuntimeServices& challenge_runtime(const char* reader)
{
    if (g_challenge_runtime == nullptr)
    {
        throw Error(std::string(reader) + " reaches the challenge runtime (DAT_004E17C8) outside a native scene");
    }
    return *g_challenge_runtime;
}

namespace
{
// 0x004231C8, stride 12: each requirement's name, the bytes 0x0004ADD0 copies from the script's record, and its
// reminder key.
constexpr std::array<u32, 0x13> kRecordSizes{0xC, 8, 8, 8, 0xC, 0xC, 8, 0x14, 0xC, 0xC, 0x10, 0xC, 8, 0xC, 0xC, 8,
    0xC, 0xC, 8};

i32 stat(u32 address)
{
    return script_statistic_at_address(address); // The DAT_00512B78 tallies, read by address.
}

i32 kills_of(u16 tag)
{
    return tag == 0 ? 0 : stat(0x00512B78u + u32(tag) * 4u);
}

// Xbox 0x0012ADF0, ECX the category: the first tallies' (DAT_0052C274) ten rows of six counts from 0x0052C738.
i32 first_tally_sum(i32 category)
{
    const auto& table = g_script_tallies.first;
    const auto at = [&table](u32 address)
    {
        const u32 index = (address - 0x0052C274u) / 4u;
        if (address < 0x0052C274u || index >= table.size())
        {
            throw Error("0x0012ADF0 reads past DAT_0052C274");
        }
        return i32(table[index]);
    };
    i32 sum = 0;
    u32 cursor = 0x0052C738u + u32(category) * 0x3A24u;
    for (i32 row = 0; row < 10; ++row)
    {
        sum += at(cursor - 0x3D0u) + at(cursor - 0x2DCu) + at(cursor - 0x1E8u) + at(cursor - 0xF4u)
            + at(cursor + 0xF4u) + at(cursor);
        cursor += 0x5B8u;
    }
    return sum;
}

float gated_step(ChallengeRuntimeServices& context)
{
    return runtime_decay_enabled(context.clock) ? runtime_frame_step(context.clock) : 0.f;
}

float real_of(u32 word)
{
    return std::bit_cast<float>(word);
}

u16 tag_of(const RuntimeChallengeRecord& record)
{
    return u16(record.value);
}

i32 attacks()
{
    return stat(0x00525B84u) + stat(0x005251FCu); // DAT_00525B84 + DAT_005251FC
}

i32 bob_kills()
{
    return stat(0x005138D0u) + stat(0x00512F48u); // DAT_005138D0 + DAT_00512F48
}

i32 live_actives(ChallengeRuntimeServices& context)
{
    i32 count = 0;
    for (const RuntimeActiveActorLink* link = context.active; link != nullptr; link = link->next)
    {
        if (link->body->energy != 0) // +0x18 -> +0x94
        {
            ++count;
        }
    }
    return count;
}

// Xbox 0x00049940: the live ghoulies past the record's count.
i32 ghoulies_left(const RuntimeChallengeRecord& record, ChallengeRuntimeServices& context)
{
    return live_actives(context) - i32(record.value);
}

// Xbox 0x000499D0, ESI the record: the kills of the record's tag still to make.
i32 kills_left(const RuntimeChallengeRecord& record)
{
    const i32 kills = tag_of(record) == 1 ? first_tally_sum(0) : kills_of(tag_of(record)); // 0x0012ADF0, ECX 0
    if (i32(record.word_c) <= kills)
    {
        return 0;
    }
    return i32(record.word_c) - kills;
}

// Xbox 0x00049B00, ECX the record: the BOB kills since the record started counting.
i32 bob_kills_since(const RuntimeChallengeRecord& record)
{
    if (record.word_10 != 0)
    {
        return bob_kills() - i32(record.word_c);
    }
    return 0;
}

// The tests at 0x002FDDCC (stride 8): 0 still running, 1 passed (the record goes), 2 neutral, 3 failed.
i32 test_record(RuntimeChallengeRecord& record, ChallengeRuntimeServices& context)
{
    switch (record.op)
    {
    case 0: // challengeReqId_AttackCounter
    case 2: // challengeReqId_ButtonCounter
    case 3: // challengeReqId_DamageCounter
    case 6: // challengeReqId_GhoulyKillCounter
        return 2; // Xbox 0x00049800
    case 1: // challengeReqId_Challenge21, Xbox 0x00049B50
        return i32((context.flags >> 0x15) & 1);
    case 4: // challengeReqId_DontKill, Xbox 0x00049A20
        return (stat(0x00512B78u + u32(tag_of(record)) * 4u) != 0 ? 1 : 0) + 2;
    case 5: // challengeReqId_DontLetActorGetHit, Xbox 0x00049810
    {
        const u32 tag = u32(tag_of(record)) * 4u;
        const i32 hits = stat(0x00519FC0u + tag) - stat(0x00516690u + tag) - stat(0x0051841Cu + tag)
            - stat(0x005172F4u + tag) - stat(0x0051696Cu + tag) - stat(0x0051A0B4u + tag) - stat(0x0051BE40u + tag)
            - stat(0x0051AD18u + tag) - stat(0x0051A390u + tag) + stat(0x0051659Cu + tag);
        return i32(u32((hits < 1 ? 1 : 0) - 1) & 3u);
    }
    case 7: // challengeReqId_GhoulyKillCounterBOB, Xbox 0x00049AC0
        if (record.word_10 == 0)
        {
            record.word_c = u32(bob_kills());
            record.word_10 = (context.flags >> 0x11) & 1;
        }
        return 2;
    case 8: // challengeReqId_KillAll, Xbox 0x00049900
        return live_actives(context) <= i32(record.value) ? 1 : 0;
    case 9: // challengeReqId_KillAllType, Xbox 0x00049970
        return context.count_by_class(tag_of(record)) == 0 ? 1 : 0; // 0x00129240, DX the tag
    case 10: // challengeReqId_KillTotalType, Xbox 0x00049990
    {
        const i32 kills = tag_of(record) == 1 ? first_tally_sum(0) : kills_of(tag_of(record));
        return i32(record.word_c) <= kills ? 1 : 0;
    }
    case 0xB: // challengeReqId_LimitedAttacks, Xbox 0x00049A40
        return (i32(record.value) <= attacks() ? 1 : 0) + 2;
    case 0xC: // challengeReqId_NoDamage, Xbox 0x00049A70
        return (context.score_total != 0 ? 1 : 0) + 2;
    case 0xD: // challengeReqId_OneBOBUsage, Xbox 0x00049A90
        return (i32(record.value) < stat(0x005295A8u) + stat(0x00528C20u) ? 1 : 0) + 2;
    case 0xE: // challengeReqId_PassSSS, Xbox 0x00049B20
        if (record.value == 0)
        {
            record.value = (context.flags >> 10) & 1;
        }
        else if ((context.flags & 0x400u) == 0)
        {
            return 1;
        }
        return 0;
    case 0xF: // challengeReqId_PlayerAlive, Xbox 0x000498D0
        return context.player_flag_10() ? 3 : 2;
    case 0x10: // challengeReqId_TimeCounter, Xbox 0x00049870
        record.value = std::bit_cast<u32>(real_of(record.value) + gated_step(context));
        return 2;
    case 0x11: // challengeReqId_TimeLimit, Xbox 0x00049790
    {
        const float left = real_of(record.value) - gated_step(context);
        record.value = std::bit_cast<u32>(left);
        return left <= 0.f ? 1 : 0;
    }
    case 0x12: // challengeReqId_WeaponsOnly, Xbox 0x00049A00
        return (stat(0x005217D8u) + stat(0x00522160u) != 0 ? 1 : 0) + 2;
    default:
        throw Error("challenge requirement " + std::to_string(record.op) + " lies past the 0x002FDDC8 table");
    }
}

// Xbox 0x0012FCA0, ESI the op, EDX the tag: the requirement's scoreboard message.
u32 add_record_message(u32 op, u16 tag, ChallengeRuntimeServices& context)
{
    u32 icon = 0;
    u32 word_70 = 1;
    u32 word_78 = 0;
    switch (op)
    {
    case 0:
        icon = 0x18;
        word_70 = 2;
        break;
    case 1:
        icon = 0x1D;
        word_70 = 5;
        break;
    case 2:
        icon = 0x1A;
        word_70 = 2;
        break;
    case 3:
        icon = 0x19;
        word_70 = 3;
        break;
    case 4:
        icon = runtime_scoreboard_tag_icon(tag); // 0x00131A30
        word_78 = 1;
        break;
    case 5:
        icon = 0x1E;
        break;
    case 6:
    case 7:
        icon = 0x15;
        word_70 = 2;
        break;
    case 8:
        icon = 0x16;
        break;
    case 9:
        icon = runtime_scoreboard_tag_icon(tag);
        break;
    case 10:
        icon = runtime_scoreboard_tag_icon(tag);
        word_70 = 2;
        break;
    case 0xB:
        icon = 0x17;
        word_70 = 2;
        break;
    case 0xC:
        icon = 0x1C;
        break;
    case 0x10:
    case 0x11:
        icon = 0x14;
        word_70 = 4;
        break;
    case 0x12:
        icon = 0x1B;
        break;
    default:
        break;
    }
    RuntimeScoreboard* board = context.hud.globals.score_board; // DAT_00545AB0
    if (board == nullptr)
    {
        throw Error("0x0012FCA0 adds a challenge message before the score board (DAT_00545AB0) exists");
    }
    return add_runtime_scoreboard_message(2, icon, word_70, 0, word_78, 0, 1, *board,
        context.board_resources); // 0x001316F0(2, icon, word_70, 0, word_78, 0, 1), EBX DAT_00545AB0
}

RuntimeScoreboard& board_of(ChallengeRuntimeServices& context)
{
    RuntimeScoreboard* board = context.hud.globals.score_board;
    if (board == nullptr)
    {
        throw Error("the challenge's messages read the score board (DAT_00545AB0) before it exists");
    }
    return *board;
}

std::vector<RuntimeChallengeRecord>& records()
{
    if (g_challenge_run.records == nullptr)
    {
        throw Error("the challenge's requirements (DAT_004E17D8) are read before 0x0004A100 made them");
    }
    return *g_challenge_run.records;
}

// Xbox 0x0004AC00: each requirement's count and icon onto its scoreboard message.
void show_records(ChallengeRuntimeServices& context)
{
    auto& list = records();
    const size_t end = list.size(); // The loop's end is read once.
    for (size_t index = 0; index < end; ++index)
    {
        const RuntimeChallengeRecord& record = list[index];
        u16 tag = 0;
        bool counted = true;
        i32 count = 0;
        switch (record.op)
        {
        case 0:
            count = attacks();
            break;
        case 1:
            count = i32(context.scene_time + g_game_progress.real_1744); // CVTTSS2SI DAT_004312D0 + DAT_004E1744
            break;
        case 2:
            count = context.presses; // DAT_00505680
            break;
        case 3:
            count = i32(context.score_total); // DAT_0054597C
            break;
        case 4:
        case 9:
            tag = tag_of(record);
            counted = false;
            break;
        case 5:
        case 0xC:
        case 0x12:
            count = 0;
            break;
        case 6:
            count = stat(0x005149F8u) + stat(0x00512C6Cu) + bob_kills();
            for (const auto& other : list)
            {
                if (other.op == 4)
                {
                    count -= kills_of(tag_of(other));
                    if (count < 0)
                    {
                        count = 0;
                    }
                }
            }
            break;
        case 7:
            count = record.word_10 == 0 ? 0 : bob_kills() - i32(record.word_c);
            break;
        case 8:
            count = ghoulies_left(record, context); // 0x00049940
            break;
        case 10:
            count = kills_left(record); // 0x000499D0
            tag = tag_of(record);
            break;
        case 0xB:
        {
            const i32 left = i32(record.value) - attacks();
            count = 0 < left ? left : 0;
            break;
        }
        case 0x10:
        case 0x11:
        {
            const float seconds = real_of(record.value);
            count = seconds > 0.f ? i32(seconds) : 0;
            break;
        }
        default:
            continue;
        }
        if (counted && -1 < count)
        {
            set_runtime_scoreboard_count(record.message, count, board_of(context),
                context.preset); // 0x001318A0, ECX +4, EDX the count
        }
        if (tag != 0)
        {
            set_runtime_scoreboard_icon(record.message, runtime_scoreboard_tag_icon(tag), board_of(context),
                context.board_resources); // 0x00131920(0x00131A30(the tag)), ECX +4
        }
    }
}

// Xbox 0x0004AB40: whether the run is over. A passed requirement is removed and passes the run (mode 3, unless it
// already failed); a failed one fails it (mode 2) and ends it at once. With requirements left, the run is over only
// when every one is neutral.
bool records_finished(ChallengeRuntimeServices& context)
{
    auto& list = records();
    i32 neutral = 0;
    for (size_t index = 0; index < list.size(); ++index)
    {
        const i32 result = test_record(list[index], context); // (*0x002FDDCC[op * 2])(record)
        if (result == 1)
        {
            if (g_challenge_run.mode != 2)
            {
                g_challenge_run.mode = 3;
            }
            list.erase(list.begin() + std::ptrdiff_t(index)); // 0x000F71F0 over the rest, the cursor back a record
            --index;
        }
        else if (result == 2)
        {
            ++neutral;
        }
        else if (result == 3)
        {
            g_challenge_run.mode = 2;
            return true;
        }
    }
    const i32 left = i32(list.size());
    return left == 0 || left == neutral;
}

// Xbox 0x0004B330, EDX the op: the first requirement of that op.
RuntimeChallengeRecord& find_record(u32 op)
{
    for (auto& record : records())
    {
        if (record.op == op)
        {
            return record;
        }
    }
    throw Error("0x0004B330 finds no challenge requirement " + std::to_string(op));
}

i32 tier_of(i32 score)
{
    return dialogue_score_tier(score, g_challenge_selection.thresholds); // 0x0004A400, ECX DAT_004E17DC
}

// Xbox 0x0012DD80: the score caption's pool (DAT_00433B4C) goes and DAT_0050EA9C loses bit 0x10000.
void release_score_caption(ChallengeRuntimeServices& context)
{
    auto& globals = context.hud.globals;
    if (globals.caption_pool != nullptr)
    {
        release_dialogue_text_pool(globals.caption_pool, *context.text_pools); // 0x0005F460
        globals.caption_pool = nullptr;
    }
    context.flags &= 0xFFFEFFFFu;
}

// Xbox 0x0004AE80: the run's score (DAT_004E18F0) and trophy (DAT_004E18F4) by kind.
void score_run(ChallengeRuntimeServices& context)
{
    auto& run = g_challenge_run;
    run.score = -1;
    run.tier = 1;
    const auto rate = [&run](i32 score)
    {
        run.score = score;
        run.tier = tier_of(score);
    };
    switch (run.kind)
    {
    case 0: // challengeId_Attacks
        if (run.mode == 3)
        {
            rate(attacks());
            return;
        }
        break;
    case 1: // challengeId_Buttons
        rate(context.presses);
        return;
    case 2: // challengeId_Challenge21: the time, always platinum
        run.score = i32(context.scene_time + g_game_progress.real_1744);
        run.tier = 6;
        return;
    case 3: // challengeId_Damage
        run.score = i32(context.score_total);
        release_score_caption(context); // 0x0012DD80
        break;
    case 4: // challengeId_Health
    {
        const i32 energy = context.player_energy();
        rate(energy < 0 ? 0 : energy);
        return;
    }
    case 5: // challengeId_Kills
    {
        i32 kills = stat(0x005149F8u) + stat(0x00512C6Cu) + bob_kills();
        run.score = kills;
        for (const auto& record : records())
        {
            if (record.op == 4)
            {
                kills -= kills_of(tag_of(record));
                run.score = kills;
            }
        }
        rate(kills < 0 ? 0 : kills);
        return;
    }
    case 6: // challengeId_KillsBOB
        rate(bob_kills_since(find_record(7)));
        return;
    case 8: // challengeId_Time: only a passed run is timed
        if (run.mode != 3)
        {
            break;
        }
        [[fallthrough]];
    case 7: // challengeId_Survive
    {
        const float seconds = real_of(find_record(0x10).value);
        rate(seconds > 0.f ? i32(seconds) : 0);
        return;
    }
    default:
        break;
    }
    run.tier = tier_of(run.score); // LAB_0004afe8
}

// Xbox 0x000F2E20: the challenges' best scores and trophies into the current game's save, and its write queued.
void save_run(ChallengeRuntimeServices& context)
{
    auto& profile = context.profile;
    if (profile.name[0] == 0) // DAT_00506C90
    {
        return;
    }
    const char16_t* name = profile.name.data();
    store_save_game_words_1b4(name, g_game_progress.words_18f8); // 0x0010E680, EBX DAT_004E18F8
    store_save_game_words_208(name, g_game_progress.words_194c); // 0x0010E740, EBX DAT_004E194C
    if (save_game_word(name, 0x184) != 0) // 0x0010DC20
    {
        clear_save_game_word_184(name); // 0x0010E2C0
    }
    queue_save_game(name, [](const void* argument)
    {
        show_runtime_corrupt_game(static_cast<const char16_t*>(argument)); // 0x000F32F0
    }, context.saves); // 0x0010DA80, EAX DAT_00506C90
}

i32 progress_word(size_t index)
{
    return i32(read_u32(g_game_progress.words_194c, index * 4)); // DAT_004E194C
}

void page(const char* key, ChallengeRuntimeServices& context)
{
    auto& dialogue = context.dialogue;
    if (i32(dialogue.state.count) + 1 < 9) // DAT_004EE1E8
    {
        request_namespaced_localtext("aid_loctext_ghoulies_dialog_dialogs", "dialogs", key,
            dialogue.measurement.wide_scratch, dialogue.measurement.localtext); // 0x000FF0A0 into DAT_00507E78
        enqueue_runtime_dialogue(dialogue.measurement.wide_scratch, 0, dialogue); // 0x000582E0
    }
}

// Xbox 0x0004B070: the best score and trophy kept, the save written, and the awards page
// ("challengeawards_" + the key + the trophy's suffix), then the unlock pages.
void award_run(ChallengeRuntimeServices& context)
{
    auto& run = g_challenge_run;
    const i32 selected = g_challenge_selection.selected; // DAT_00506578
    const auto& thresholds = g_challenge_selection.thresholds; // DAT_004E17DC
    if (run.score != -1)
    {
        auto best = std::span<u8>(g_game_progress.words_18f8);
        const i32 old = i32(read_u32(best, size_t(selected) * 4)); // DAT_004E18F8
        // Higher is better when the bronze score is below the silver one, otherwise lower; an unset best (-1)
        // always takes the score.
        const bool better = thresholds[0] < thresholds[1] ? old < run.score : run.score < old;
        if (better || old == -1)
        {
            std::memcpy(best.data() + size_t(selected) * 4, &run.score, 4);
        }
    }
    if (run.kind == 2)
    {
        const i32 platinum = 6;
        std::memcpy(g_game_progress.words_194c.data() + 0x50, &platinum, 4); // DAT_004E199C, challenge 21's trophy
    }
    else if (progress_word(size_t(selected)) < run.tier)
    {
        std::memcpy(g_game_progress.words_194c.data() + size_t(selected) * 4, &run.tier, 4);
    }
    save_run(context); // 0x000F2E20
    static constexpr std::array<const char*, 7> kSuffixes{"_unplayed", "_failed", "_none", "_bronze", "_silver",
        "_gold", "_platinum"}; // PTR_s__unplayed_004232d0
    std::string key = "challengeawards_"; // PTR_s_challengeawards__004232ec into DAT_00508EB8
    key += g_challenge_selection.key.data(); // DAT_004E17EC
    key += kSuffixes.at(size_t(run.tier));
    page(key.c_str(), context);
    u8& progress = context.profile.progress.bytes[4]; // *(DAT_00506C6C+8)+4
    const auto count_tier = [](i32 tier)
    {
        i32 count = 0;
        for (size_t index = 0; index < 0x15; ++index)
        {
            if (progress_word(index) == tier)
            {
                ++count;
            }
        }
        return count;
    };
    if ((progress & 0x20) == 0 && 0x13 < count_tier(6) + count_tier(5))
    {
        page("challenge_challenge21", context);
        progress |= 0x20;
    }
    if ((progress & 0x10) == 0 && count_tier(6) == 1)
    {
        page("challenge_firstplatinum", context);
        progress |= 0x10;
    }
    if ((progress & 0x40) == 0 && progress_word(0x14) == 6)
    {
        page("challenge_platinum21", context);
        progress |= 0x40;
    }
}

// Xbox 0x00049C00: challenge 21's scene order (flag 8) given up.
void release_challenge_order(ChallengeRuntimeServices& context)
{
    if ((context.scene_flags & 8) == 0 && (context.flags & 0x200000u) == 0)
    {
        return;
    }
    context.scene_flags &= 0xF7;
    g_challenge_selection.order[0] = 0; // DAT_004E1748
    context.hold_order(g_challenge_selection.order.data()); // 0x001234B0, EAX DAT_004E1748
}

// Xbox 0x00049C80: challenge 21 from its first scene again.
void restart_challenge_order(ChallengeRuntimeServices& context)
{
    const bool held = (context.scene_flags & 8) != 0;
    if (!held && (context.flags & 0x200000u) == 0)
    {
        return;
    }
    std::array<char, 0x80> order{};
    std::strcpy(order.data(), g_challenge_selection.order.data());
    if (held || (context.flags & 0x200000u) != 0)
    {
        g_game_progress.word_1740 = 0; // DAT_004E1740
        g_challenge_selection.order[0] = 0;
    }
    g_game_progress.real_1744 = 0.f - context.scene_time; // DAT_004E1744
    hold_runtime_challenge_order(order.data(), context.scene_flags, context.flags, context.hold_order); // 0x00049B60
    context.request_script(context.held_order(), 1); // 0x0011EB30(1), EAX DAT_00511B64
}

}

// Xbox 0x0004A310: the challenge again.
void restart_run(ChallengeRuntimeServices& context)
{
    auto& globals = context.hud.globals;
    if (globals.caption_pool != nullptr) // DAT_00433B4C
    {
        release_dialogue_text_pool(globals.caption_pool, *context.text_pools); // 0x0005F460
        globals.caption_pool = nullptr;
    }
    context.flags &= 0xFFFEFFFFu;
    if ((context.scene_flags & 8) != 0 || (context.flags & 0x200000u) != 0)
    {
        restart_challenge_order(context); // 0x00049C80
        return;
    }
    context.request_script(context.scene_script, 1); // 0x0011EB30(1), EAX DAT_00510E58
}

namespace
{

// The reaction 0x0004A4D0 state 6 and 0x0004A990 send: message 0x4D, 0x1D for a passed run, else 0x1C.
void react(u32 actor, ChallengeRuntimeServices& context)
{
    std::array<u32, 9> message{0x4D, g_challenge_run.mode == 3 ? 0x1Du : 0x1Cu};
    context.message_player(actor, message); // Vtable +0xC
}

float reaction_delay(ChallengeRuntimeServices& context)
{
    u32& random = context.dialogue.random; // DAT_0054DD7C
    random = random * 0x19660Du + 0x3C6EF35Fu;
    const float unit = std::bit_cast<float>((random & 0x7FFFFFu) | 0x3F800000u);
    return (unit - 1.f) * 10.f + 5.f; // DAT_0032B0F8, DAT_0032B10C, DAT_0032B184
}

// Xbox 0x0004A990: each player's reaction repeats when its timer runs out; the menu is done once chosen.
bool tick_restart_menu(ChallengeRuntimeServices& context)
{
    auto& run = g_challenge_run;
    for (auto& reaction : *run.reactions)
    {
        const float step = runtime_frame_step(context.clock);
        bool due = false;
        if (0.f < reaction.timer)
        {
            const float left = reaction.timer - step;
            reaction.timer = left;
            if (left <= 0.f)
            {
                reaction.timer = 0.f;
                due = true;
            }
        }
        if (!due && !(reaction.timer < 0.001f)) // DAT_0032B1A4
        {
            continue;
        }
        if (context.player_goal_held(reaction.actor))
        {
            continue;
        }
        context.drain_player_goals(reaction.actor); // 0x00028240 until +0x980 is empty
        react(reaction.actor, context);
        reaction.timer = reaction_delay(context);
    }
    return run.choice != 0;
}

void release_restart_input(ChallengeRuntimeServices& context)
{
    auto& run = g_challenge_run;
    auto& dialogue = context.dialogue;
    pop_runtime_input_entry(dialogue.input); // 0x001093C0
    release_runtime_input_device(run.input, dialogue.input_release); // 0x00108230, EAX DAT_004E19A4
    dialogue.input_views.erase(run.input_address);
    dialogue.input_addresses.erase(run.input_address);
    run.input = {};
    run.input_address = 0;
}
}

// Xbox 0x00049B60, EAX the order
void hold_runtime_challenge_order(const char* order, u8& scene_flags, u32 flags,
    const std::function<void(const char*)>& hold)
{
    if ((scene_flags & 8) != 0 || (flags & 0x200000u) != 0)
    {
        return;
    }
    scene_flags |= 8;
    std::strcpy(g_challenge_selection.order.data(), order); // DAT_004E1748
    hold(g_challenge_selection.order.data()); // 0x001234B0, EAX DAT_004E1748
}

// Xbox 0x0004A100
void create_runtime_challenge(u16 kind, ChallengeRuntimeServices& context)
{
    auto& run = g_challenge_run;
    run.records = std::make_unique<std::vector<RuntimeChallengeRecord>>(); // 0x000F72D0(0x38): stride 0x14, two
    run.records->reserve(2);
    run.kind = kind;
    RuntimeChallengeRecord record{};
    const auto rules = [&context]() // Xbox 0x0004A0E0: the player must stay alive.
    {
        RuntimeChallengeRecord alive{};
        alive.op = 0xF;
        add_runtime_challenge_record(alive, context);
        context.flags |= 0x8000u;
    };
    switch (kind)
    {
    case 0:
    case 4:
    case 7:
    case 8:
        rules(); // The 0x0004A0E0 body inline.
        return;
    case 1:
        record.op = 2; // EDX, the pool header's 2
        add_runtime_challenge_record(record, context);
        record = {};
        record.op = 0xE;
        add_runtime_challenge_record(record, context);
        rules();
        context.new_button_row = 1; // DAT_00505684
        return;
    case 2:
        record.op = 1;
        add_runtime_challenge_record(record, context);
        return;
    case 3:
        rules();
        create_runtime_score_captions(context.hud); // 0x0012DCD0
        return;
    case 5:
        record.op = 6;
        add_runtime_challenge_record(record, context);
        rules();
        return;
    case 6:
        record.op = 7;
        add_runtime_challenge_record(record, context);
        record = {};
        record.op = 0xD;
        record.value = u32(stat(0x005295A8u) + stat(0x00528C20u));
        add_runtime_challenge_record(record, context);
        rules();
        return;
    default:
        return;
    }
}

// Xbox 0x0004ADD0
void add_runtime_challenge_record(const RuntimeChallengeRecord& record, ChallengeRuntimeServices& context)
{
    auto& list = records(); // A full pool grows (0x000662A0).
    // 0x000F71F0 copies the op's size (0x004231CC + op * 12) from the script's record; the rest of the slot stays
    // zero here.
    RuntimeChallengeRecord added{};
    std::memcpy(&added, &record, kRecordSizes.at(record.op));
    u32 message = 0;
    switch (record.op)
    {
    case 4:
        if (tag_of(record) != 0)
        {
            message = add_record_message(record.op, tag_of(record), context); // EDX the tag
        }
        break;
    case 9:
    case 10:
        message = add_record_message(record.op, tag_of(record), context); // EDX the tag
        break;
    case 0xD:
    case 0xE:
    case 0xF:
        break;
    default:
        message = add_record_message(record.op, 0, context); // EDX 0
        break;
    }
    added.message = message;
    list.push_back(added);
}

// Xbox 0x0004A240
void start_runtime_challenge(ChallengeRuntimeServices& context)
{
    context.flags |= 0x4000u;
    u8& progress = context.profile.progress.bytes[0]; // **(DAT_00506C6C+8)
    if (-1 < static_cast<signed char>(progress))
    {
        g_challenge_run.reset_notice = 1; // DAT_004E1A34
        progress |= 0x80;
    }
    set_runtime_challenge_state(2, context);
}

// Xbox 0x00049BA0
void start_runtime_scene_challenge(ChallengeRuntimeServices& context)
{
    if ((context.scene_flags & 8) == 0 && (context.flags & 0x200000u) == 0)
    {
        return;
    }
    create_runtime_challenge(2, context); // 0x0004A100(2), challengeId_Challenge21
    start_runtime_challenge(context); // The 0x0004A240 body inline.
}

// Xbox 0x0004A4D0
void set_runtime_challenge_state(i32 state, ChallengeRuntimeServices& context)
{
    auto& run = g_challenge_run;
    if (run.state == 6)
    {
        run.reactions.reset(); // 0x000663E0
        context.pause(u8(0xFFFF), 1); // 0x00117400(0xFFFF, 1)
        if (run.widget != nullptr)
        {
            destroy_dialogue_widget(run.widget, context.resources,
                *context.text_pools); // 0x00142070
            run.widget = nullptr;
        }
        if (run.input.data() != nullptr)
        {
            release_restart_input(context); // 0x001093C0, 0x00108230
        }
        if (run.choice == 1)
        {
            restart_run(context); // 0x0004A310
        }
        else if (run.choice == 2)
        {
            release_challenge_order(context); // 0x00049C00
            context.next_row = 6; // DAT_00429BC0, the challenges
            size_t at = 0;
            for (; context.challenge_game[at] != 0; ++at)
            {
                context.selected_game[at] = context.challenge_game[at]; // lstrcpyW(DAT_00429BC4, DAT_004E19B4)
            }
            context.selected_game[at] = 0;
            context.request_script("aid_script_ghoulies_gameselect_gameselect", 1); // 0x0011EB30(1), 0x00322B8C
        }
    }
    switch (state)
    {
    case 1:
        run.mode = 0;
        break;
    case 2:
        // Each requirement's start (0x002FDDC8) is the bare RET 0x001628B0.
        break;
    case 3:
        run.mode = 1;
        break;
    case 4:
    {
        context.flags |= 0x1000000u;
        context.pause(u8(0xFFFF), 0); // 0x00117400(0xFFFF, 0)
        score_run(context); // 0x0004AE80
        static constexpr std::array<const char*, 7> kDitties{"XACT_SOUNDBANK_GSTREAMSDITTIES_CHALLENGE_WINNOWT",
            "XACT_SOUNDBANK_GSTREAMSDITTIES_CHALLENGE_WINNOWT", "XACT_SOUNDBANK_GSTREAMSDITTIES_CHALLENGE_WINNOWT",
            "XACT_SOUNDBANK_GSTREAMSDITTIES_CHALLENGE_BRONZE", "XACT_SOUNDBANK_GSTREAMSDITTIES_CHALLENGE_SILVER",
            "XACT_SOUNDBANK_GSTREAMSDITTIES_CHALLENGE_GOLD", "XACT_SOUNDBANK_GSTREAMSDITTIES_CHALLENGE_PLANTINUM"};
        request_audio_stream(kDitties.at(size_t(run.tier)), 8, context.streams); // 0x00038720, EAX 0x002FDF60 + tier * 0x80
        if (run.kind == 1)
        {
            run.timer = 1.f; // DAT_0032B0F8
            break;
        }
        run.timer = 2.5f; // DAT_0032B20C
        if (context.player_without_energy()) // 0x001292D0, its body +0x94
        {
            run.timer = 6.5f; // DAT_0032B68C
        }
        break;
    }
    case 5:
        score_run(context); // 0x0004AE80
        award_run(context); // 0x0004B070
        break;
    case 6:
    {
        run.reactions = std::make_unique<std::vector<RuntimeChallengeReaction>>(); // 0x000F72D0(0x60): stride 8, 10
        run.reactions->reserve(10);
        for (const u32 actor : context.reacting_players())
        {
            run.reactions->push_back({reaction_delay(context), actor});
            react(actor, context);
        }
        const float inset = 16.f; // DAT_0032B1F4
        const std::array<float, 2> anchor{context.layout.inner_left + inset, context.layout.inner_bottom - inset};
        const std::array<float, 2> row_step{0.f, -32.f}; // DAT_0032B600
        const std::array<float, 2> origin{20.f, -16.f}; // DAT_0032B234, DAT_0032B5FC
        create_resource_dialogue_widget(run.widget, 0, context.restart_rows, anchor, row_step,
            origin, nullptr, 1.f, 0.75f, "menutext", "aid_loctext_ghoulies_menu_menutext", 0x400C0,
            context.dialogue.allocation); // 0x00141DA0, ESI DAT_004E19A0, 0x002FE2E0
        auto& dialogue = context.dialogue;
        run.input = create_runtime_selected_input(dialogue.input_release); // 0x00107D60, ESI DAT_004E19A4
        run.input_address = static_cast<u32>(reinterpret_cast<std::uintptr_t>(run.input.data()));
        dialogue.input_addresses[run.input_address] = run.input;
        dialogue.input_views[run.input_address] = {run.input, {nullptr, nullptr, nullptr, nullptr}};
        push_runtime_input_entry(run.input_address, "Challenge restart", dialogue.input); // 0x00109340, ESI 0x00322B78
        ++dialogue.pause_count; // DAT_00510CEC
        run.hold = 1; // DAT_004E19AC
        break;
    }
    default:
        break;
    }
    run.state = state;
}

// Xbox 0x00049DE0
void tick_runtime_challenge(ChallengeRuntimeServices& context)
{
    auto& run = g_challenge_run;
    switch (run.state)
    {
    case 2:
        set_runtime_challenge_state(3, context);
        return;
    case 3:
        show_records(context); // 0x0004AC00
        if (records_finished(context)) // 0x0004AB40
        {
            set_runtime_challenge_state(4, context);
        }
        return;
    case 4:
    {
        show_records(context);
        const float step = runtime_frame_step(context.clock);
        if (0.f < run.timer)
        {
            run.timer -= step;
            if (run.timer <= 0.f)
            {
                run.timer = 0.f;
                set_runtime_challenge_state(5, context);
                return;
            }
        }
        if (run.timer < 0.001f) // DAT_0032B1A4
        {
            set_runtime_challenge_state(5, context);
        }
        return;
    }
    case 5:
        if (context.dialogue.state.mode == 0) // DAT_004EE1EC
        {
            set_runtime_challenge_state(6, context);
        }
        return;
    case 6:
    {
        show_records(context);
        auto& dialogue = context.dialogue;
        refresh_runtime_input_device(run.input_address, dialogue.frame, dialogue.input,
            dialogue.input_release); // 0x00109470, EAX DAT_004E19A4
        update_dialogue_widget(*run.widget, &dialogue.input_views.at(run.input_address),
            dialogue.widget_update); // 0x00142460, EDI DAT_004E19A0, EAX DAT_004E19A4
        if (tick_restart_menu(context)) // 0x0004A990
        {
            set_runtime_challenge_state(1, context);
        }
        return;
    }
    default:
        return;
    }
}

// Xbox 0x00049FF0
void end_runtime_challenge(ChallengeRuntimeServices& context)
{
    auto& run = g_challenge_run;
    run.records.reset(); // 0x000663E0
    if (run.widget != nullptr)
    {
        destroy_dialogue_widget(run.widget, context.resources,
            *context.text_pools); // 0x00142070
        run.widget = nullptr;
    }
    if (run.input.data() != nullptr)
    {
        auto& dialogue = context.dialogue;
        if (runtime_input_is_selected(run.input, run.input_address, dialogue.input)) // 0x00109620
        {
            pop_runtime_input_entry(dialogue.input); // 0x001093C0
        }
        // The device's kind (+0) picks its release: 1..3 0x00107DD0, 4 0x00107960, 5 0x00107B60.
        switch (read_u32(run.input, 0))
        {
        case 1:
        case 2:
        case 3:
            release_runtime_simple_input(run.input);
            break;
        case 4:
            release_runtime_composite_input(run.input, dialogue.input_release);
            break;
        case 5:
            release_runtime_actor_input(run.input, dialogue.input_release);
            break;
        default:
            break;
        }
        dialogue.input_views.erase(run.input_address);
        dialogue.input_addresses.erase(run.input_address);
        run.input = {};
        run.input_address = 0;
    }
    context.flags &= 0xFEFF3FFFu;
    if (run.state != 1)
    {
        set_runtime_challenge_state(1, context); // 0x0004A4D0
    }
    if (run.hold != 0)
    {
        --context.dialogue.pause_count; // DAT_00510CEC
        run.hold = 0;
    }
    run.reactions.reset(); // 0x000663E0
}

// Xbox 0x0004B350
void choose_runtime_challenge_restart()
{
    g_challenge_run.choice = 1; // DAT_004E19A8
}

// Xbox 0x0004B360
void choose_runtime_challenge_quit()
{
    g_challenge_run.choice = 2;
}
}
