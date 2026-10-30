#include "final_score.h"
#include "frontend_state.h"
#include "../scene/gameselect_control.h"
#include "../scene/scene_controls.h"
#include "../transitions/native_book.h"
#include "dialogue_activation.h"
#include "dialogue_draw_outer.h"
#include "dialogue_draw_context.h"
#include "dialogue_text_metrics.h"
#include "dialogue_text_pool.h"
#include "dialogue_time.h"
#include "dialogue_widget_construction.h"
#include "dialogue_widget_state.h"
#include "hud_layout.h"
#include "button_bat_menu.h"
#include "gameselect_rows.h"
#include "input_activation.h"
#include "../../gbtg/text/localtext_resources.h"
#include "../audio/bank_streams.h"
#include "../audio/event_voices.h"
#include "../camera/runtime_display_setup.h"
#include "../camera/view_initialization.h"
#include "../cutscenes/actor_tracks.h"
#include "../cutscenes/event_runtime.h"
#include "../entities/actor_render_registration.h"
#include "../saves/runtime_profile.h"
#include "../saves/save_games.h"
#include "../saves/save_summary.h"
#include "../scene/executable_resources.h"
#include "gbtg/anim.h"
#include "gbtg/resource_error.h"

#include <bit>
#include <cstdio>
#include <cstring>
#include <string>

namespace gbtg
{
namespace
{
FinalScoreState& state_of(FinalScoreServices& services)
{
    if (services.state == nullptr)
    {
        throw Error("the final score (DAT_00506568) is read before 0x000D3E30 builds it");
    }
    return *services.state;
}

FinalScoreTally& tally_of(FinalScoreServices& services)
{
    if (services.tally == nullptr)
    {
        throw Error("the final score tally (DAT_00506558) is read before 0x000D3E30 builds it");
    }
    return *services.tally;
}

float frame_step(const GameSelectControlServices& s)
{
    if (s.freeze <= 0.f) // DAT_004312DC
    {
        return s.dt * s.scene_scale; // DAT_005096C0 * DAT_004312C4
    }
    return 0.f;
}

i32 exe_int(const GameSelectControlServices& s, u32 address)
{
    return i32(read_u32(s.image.memory(address, 4), 0));
}

const char16_t* current_name(FinalScoreServices& services)
{
    return state_of(services).name.data(); // +4
}

// The stat the row's callback reads: the empty name's default, or the save's word (0x0010DCA0..0x0010DD60, EBX
// the name).
i32 row_stat(size_t offset, i32 nameless, FinalScoreServices& services)
{
    const char16_t* name = current_name(services);
    if (name == nullptr || name[0] == 0)
    {
        return nameless;
    }
    return i32(save_game_word(name, offset));
}

// The callbacks' loop from rank 25 down: the first threshold the stat meets, else 1.
template <typename Meets>
u16 rank_of(const std::array<i32, 25>& thresholds, i32 stat, Meets meets)
{
    u16 rank = 0;
    for (i32 index = 0x18; index >= 0; --index)
    {
        if (meets(thresholds[size_t(index)], stat))
        {
            rank = u16(index + 1);
            break;
        }
    }
    if (rank == 0)
    {
        rank = 1;
    }
    return rank;
}

// 0x00166B36: _itow, the CRT's __itoa into 40 bytes widened by 0x0016AD59.
void format_wide_integer(i32 value, char16_t* output, i32 radix)
{
    char digits[40]{};
    if (radix != 10)
    {
        throw Error("0x00166B36 with a radix other than 10 is not translated");
    }
    std::snprintf(digits, sizeof(digits), "%d", value);
    size_t index = 0;
    for (; digits[index] != 0; ++index)
    {
        output[index] = char16_t(u8(digits[index]));
    }
    output[index] = 0;
}

using FinalScoreRowRank = void (*)(u16& rank, char16_t* value, FinalScoreServices& services);

// The 0x00303F60 rows' +0x10C callbacks.
FinalScoreRowRank row_rank(u32 address)
{
    switch (address)
    {
    case 0x000D2950:
        return rank_runtime_final_score_time;
    case 0x000D2B00:
        return rank_runtime_final_score_kills;
    case 0x000D2CB0:
        return rank_runtime_final_score_faints;
    case 0x000D2E10:
        return rank_runtime_final_score_prisoners;
    default:
        throw Error("final score row callback " + std::to_string(address) + " is not translated");
    }
}

// The page table at 0x00304EC4, stride 0x14: start, free, tick, draw and done.
using FinalScorePageStep = void (*)(FinalScoreServices& services);
using FinalScorePageDone = bool (*)(FinalScoreServices& services);
struct FinalScorePage
{
    FinalScorePageStep start; // +0
    FinalScorePageStep free; // +4, 0x00304EB4 + page * 0x14
    FinalScorePageStep tick; // +8, 0x00304EB8 + page * 0x14
    FinalScorePageStep draw; // +0xC, 0x00304EBC + page * 0x14
    FinalScorePageDone done; // +0x10, 0x00304EC0 + page * 0x14
};

template <u32 Address>
void untranslated_page_step(FinalScoreServices&)
{
    char text[96];
    std::snprintf(text, sizeof(text), "0x%08X (a final score page step) is not translated", Address);
    throw Error(text);
}

template <u32 Address>
bool untranslated_page_done(FinalScoreServices&)
{
    char text[96];
    std::snprintf(text, sizeof(text), "0x%08X (a final score page's done test) is not translated", Address);
    throw Error(text);
}

// 0x001628B0 returns at once.
void empty_page_step(FinalScoreServices&)
{
}

const FinalScorePage& page_of(i32 page)
{
    static const FinalScorePage pages[5]{
        {start_runtime_final_score_intro, free_runtime_final_score_intro, tick_runtime_final_score_intro,
            empty_page_step, runtime_final_score_intro_done},
        {start_runtime_final_score_scores, free_runtime_final_score_scores, tick_runtime_final_score_scores,
            draw_runtime_final_score_scores, runtime_final_score_scores_done},
        {start_runtime_final_score_super_scary, free_runtime_final_score_super_scary,
            tick_runtime_final_score_super_scary, draw_runtime_final_score_super_scary,
            runtime_final_score_super_scary_done},
        {untranslated_page_step<0x000D2FE0>, untranslated_page_step<0x000D3160>, untranslated_page_step<0x000D3210>,
            untranslated_page_step<0x000D3310>, untranslated_page_done<0x000D3350>},
        {untranslated_page_step<0x000D35A0>, untranslated_page_step<0x000D3780>, untranslated_page_step<0x000D3860>,
            untranslated_page_step<0x000D38E0>, untranslated_page_done<0x000D3970>}};
    return pages[page - 1];
}

float exe_real(const GameSelectControlServices& s, u32 address)
{
    return std::bit_cast<float>(read_u32(s.image.memory(address, 4), 0));
}

FinalScoreScoresPage& scores_of(FinalScoreServices& services)
{
    if (services.scores == nullptr)
    {
        throw Error("final score page 2 (DAT_00506554) is read before 0x000D1540 makes it");
    }
    return *services.scores;
}

// 0x0005F580: 0x0005F5C0 with the record's centring (EAX), no panel, and sticky (the eighth argument, 1).
u32 add_text(FinalScoreScoresPage& page, const char16_t* text, float x, float y, float scale, i32 centered,
    GameSelectControlServices& s)
{
    return create_dialogue_text_record(*page.pool, text, x, y, scale, scale, 1.f, centered, 0, 1, s.metrics,
        s.viewport_width, s.next_text_id, s.random);
}

u32 add_line(FinalScoreScoresPage& page, FinalScoreTextLine& line, float scale, i32 centered,
    GameSelectControlServices& s)
{
    return add_text(page, line.text.data(), line.x, line.y, scale, centered, s);
}

// The icon texture's +8 width and +0xA height.
float icon_width(const FinalScoreScoreRow& row)
{
    return float(read_u16(row.icon, 8));
}

float icon_height(const FinalScoreScoreRow& row)
{
    return float(read_u16(row.icon, 0xA));
}

// 0x00039690 inline (DAT_004E0CE8 + voice * 0x24, stack 0), then the voice word cleared.
void stop_voice(i16& voice, GameSelectControlServices& s)
{
    if (voice != -1)
    {
        stop_audio_event_voice(voice, 0, s.voices);
    }
    voice = -1;
}

bool bat_arrived(Actor* bat, GameSelectControlServices& s)
{
    if (bat == nullptr)
    {
        throw Error("0x0006DB10 reads final score page 2's bat (+0x2134) before it exists");
    }
    // 0x0006DB10, EAX the bat: +0x934 is 1 or 3.
    const i32 state = s.bat_screen(*bat).state;
    return state == 1 || state == 3;
}

// The current page's free, through 0x00304EB4 + page * 0x14 for pages 1..5.
void free_current_page(FinalScoreState& state, FinalScoreServices& services)
{
    if (0 < state.page && state.page < 6)
    {
        page_of(state.page).free(services);
    }
}
}

// Xbox 0x00082D50
void final_score_control_object_sizes(ObjectSizes& output)
{
    output.instance = 0x28;
    output.parameters = 8;
}

// Xbox 0x000D2FA0
void clear_runtime_final_score_tally(FinalScoreTally& tally)
{
    for (auto& row : tally.rows)
    {
        row.name[0] = 0;
        row.icon[0] = 0;
        row.rank = 0;
        row.rank_count = 0;
        row.value[0] = 0;
    }
}

// Xbox 0x000D2F10
void fill_runtime_final_score_row(i32 index, FinalScoreServices& services)
{
    const auto& image = services.frontend.image;
    auto& row = tally_of(services).rows[size_t(index)];
    const u32 entry = 0x00303F60u + u32(index) * 0x110u;
    std::strcpy(row.name.data(), image.string(entry));
    const u32 icon = read_u32(image.memory(entry + 0x100, 4), 0);
    std::strcpy(row.icon.data(), image.string(0x003043A0u + icon * 0x80u));
    const u32 sound = read_u32(image.memory(entry + 0x104, 4), 0);
    row.sound = image.string(0x003045A0u + sound * 0x80u);
    row_rank(read_u32(image.memory(entry + 0x10C, 4), 0))(row.rank, row.value.data(), services);
    row.rank_count = u16(read_u32(image.memory(entry + 0x108, 4), 0));
}

// Xbox 0x000D2950
void rank_runtime_final_score_time(u16& rank, char16_t* value, FinalScoreServices& services)
{
    const auto& s = services.frontend;
    const i32 base = exe_int(s, 0x00428AF8); // 180 minutes
    const i32 step = exe_int(s, 0x00428AFC); // 25 minutes
    std::array<i32, 25> thresholds{};
    for (i32 index = 0; index < 25; ++index)
    {
        thresholds[size_t(index)] = (base + step * (0x18 - index)) * 0x3C;
    }
    const i32 seconds = row_stat(0x18C, 1, services); // 0x0010DCA0
    rank = rank_of(thresholds, seconds, [](i32 threshold, i32 stat)
    {
        return stat <= threshold;
    });
    append_dialogue_seconds(value, float(seconds), 1, s.localtext); // 0x000FB860
}

// Xbox 0x000D2B00
void rank_runtime_final_score_kills(u16& rank, char16_t* value, FinalScoreServices& services)
{
    const auto& s = services.frontend;
    const i32 base = exe_int(s, 0x00428B00); // 700
    const i32 step = exe_int(s, 0x00428B04); // 20
    std::array<i32, 25> thresholds{};
    for (i32 index = 0; index < 25; ++index)
    {
        thresholds[size_t(index)] = base - step * (0x18 - index);
    }
    const i32 kills = row_stat(0x190, 1, services); // 0x0010DCE0
    rank = rank_of(thresholds, kills, [](i32 threshold, i32 stat)
    {
        return threshold <= stat;
    });
    format_wide_integer(kills, value, 10); // 0x00166B36
}

// Xbox 0x000D2CB0
void rank_runtime_final_score_faints(u16& rank, char16_t* value, FinalScoreServices& services)
{
    const auto& s = services.frontend;
    const i32 base = exe_int(s, 0x00428B08); // 20
    const i32 step = exe_int(s, 0x00428B0C); // 10
    std::array<i32, 25> thresholds{};
    for (i32 index = 0; index < 25; ++index)
    {
        thresholds[size_t(index)] = base + step * (0x18 - index);
    }
    const i32 faints = row_stat(0x194, 0, services); // 0x0010DD20
    rank = rank_of(thresholds, faints, [](i32 threshold, i32 stat)
    {
        return stat <= threshold;
    });
    format_wide_integer(faints, value, 10); // 0x00166B36
}

// Xbox 0x000D2E10
void rank_runtime_final_score_prisoners(u16& rank, char16_t* value, FinalScoreServices& services)
{
    static constexpr std::array<i32, 25> thresholds{1, 1, 2, 2, 2, 3, 3, 4, 4, 4, 5, 5, 6, 6, 6, 7, 7, 8, 8, 8, 9,
        9, 10, 10, 10};
    const i32 prisoners = row_stat(0x198, 0, services); // 0x0010DD60
    rank = rank_of(thresholds, prisoners, [](i32 threshold, i32 stat)
    {
        return threshold <= stat;
    });
    format_wide_integer(prisoners, value, 10); // 0x00166B36
}

// Xbox 0x000D2880
void build_runtime_final_score_tally(FinalScoreServices& services)
{
    auto& tally = tally_of(services);
    const char16_t* name = current_name(services); // DAT_00506568 + 4
    clear_runtime_final_score_tally(tally); // 0x000D2FA0
    for (i32 index = 0; index < 4; ++index)
    {
        fill_runtime_final_score_row(index, services); // 0x000D2F10, ECX the row
    }
    tally.total = 0;
    tally.total = tally.rows[0].rank;
    tally.total = tally.total + tally.rows[1].rank;
    tally.total = tally.total + tally.rows[2].rank;
    tally.total = tally.rows[3].rank + tally.total;
    if (name != nullptr && name[0] != 0)
    {
        // The inline save lookup over DAT_005462E4: the named save's +0x19C, or 0.
        tally.save_19c = save_game_word(name, 0x19C);
        return;
    }
    tally.save_19c = 0;
}

// Xbox 0x000D2830
void free_runtime_final_score_tally(FinalScoreServices& services)
{
    services.tally.reset(); // HeapFree(DAT_00506558)
}

// Xbox 0x000D3E30
void create_runtime_final_score(FinalScoreServices& services)
{
    auto& s = services.frontend;
    services.state = std::make_unique<FinalScoreState>(); // 0x000F72D0(0x1FC)
    auto& state = *services.state;
    state.cutscene = nullptr;
    state.cutscene_on = 0;
    state.text = {};
    state.input = {};
    state.page = 0;
    state.name[0] = 0;
    std::strcpy(state.ditty.data(), "XACT_SOUNDBANK_GSTREAMSDITTIES_PAUSE"); // 0x003249D4
    state.ditty_playing = 0;
    state.cutscene_kind = 0;
    initialize_camera_view(state.camera, "finalscore", 8); // 0x000496C0, EDX +0x10C, 0x003233B0
    state.camera.aspect_slot = 0; // +0x30
    i32 fit_w = 0;
    i32 fit_x = 0;
    i32 fit_h = 0;
    i32 fit_y = 0;
    float fov = 0.f;
    camera_fit_display_view(state.camera.focal, 0, fit_w, fit_x, fit_h, fit_y, fov, state.camera.pixel_aspect,
        state.camera.focal_pixels); // 0x000F57F0
    link_camera_view(0, state.camera); // The inline 0x00048C20 link and its 0x00048CF0 when slot 0 has no view.
    camera_bind_named_view(0, "finalscore"); // 0x00048CF0, EBX 0x003233B0
    state.text = acquire_animation_resource(s.resources,
        "aid_loctext_ghoulies_finalscore_finalscoretext"); // 0x00035E90, 0x00304C30
    if (state.text.data() == nullptr)
    {
        resource_error(s.error, 2); // 0x00032700(2)
    }
    state.input = create_runtime_keyboard_input(s.input_release); // 0x00107EF0, ESI +0x170
    services.input_address = s.map_input(state.input); // The scene's input registry; 0x00142460 reads the device.
    services.tally = std::make_unique<FinalScoreTally>(); // 0x000F72D0(0xE28)
    services.tally->total = 0;
    services.tally->save_19c = 0;
    clear_runtime_final_score_tally(*services.tally); // 0x000D2FA0
}

// Xbox 0x000D44D0
void start_runtime_final_score_ditty(FinalScoreServices& services)
{
    auto& state = state_of(services);
    if (state.ditty_playing == 0)
    {
        acquire_tracked_audio_bank(state.ditty.data(), 1, services.frontend.streams); // 0x00038910, EAX +0x174
        state.ditty_playing = 1;
    }
}

// Xbox 0x000D4500
void stop_runtime_final_score_ditty(FinalScoreServices& services)
{
    auto& state = state_of(services);
    if (state.ditty_playing != 0)
    {
        // 0x00038970 inline.
        release_tracked_audio_bank(state.ditty.data(), services.frontend.streams);
        state.ditty_playing = 0;
    }
}

// Xbox 0x000D3FB0
void destroy_runtime_final_score(FinalScoreServices& services)
{
    auto& s = services.frontend;
    auto& state = state_of(services);
    free_current_page(state, services);
    free_runtime_final_score_tally(services); // 0x000D2830
    stop_runtime_final_score_ditty(services); // 0x000D4500
    if (state.input.data() != nullptr && read_u32(state.input, 4) != 0)
    {
        release_runtime_input_device(state.input, s.input_release); // 0x00108230
        s.unmap_input(services.input_address);
        services.input_address = 0;
        state.input = {};
    }
    release_animation_resource(s.resources, state.text); // 0x00035C00
    if (state.cutscene != nullptr)
    {
        release_runtime_cutscene(state.cutscene, s.cutscenes); // 0x0004D720
        state.cutscene = nullptr;
    }
    camera_unlink_view(&state.camera); // 0x00048C70
    services.state.reset(); // HeapFree(DAT_00506568)
}

// Xbox 0x000D4280
void set_runtime_final_score_cutscene(i32 on, i32 kind, FinalScoreServices& services)
{
    auto& s = services.frontend;
    auto& state = state_of(services);
    if (on != 0 && state.cutscene_on != 0)
    {
        if (kind == state.cutscene_kind)
        {
            return;
        }
        if (state.cutscene_on != 0 && state.cutscene_kind != kind)
        {
            set_runtime_final_score_cutscene(0, 0, services);
        }
    }
    state.cutscene_on = on;
    state.cutscene_kind = kind;
    if (state.cutscene_on == 0)
    {
        if (state.cutscene != nullptr)
        {
            release_runtime_cutscene(state.cutscene, s.cutscenes); // 0x0004D720
            state.cutscene = nullptr;
        }
        return;
    }
    if (state.cutscene != nullptr)
    {
        return;
    }
    // The gold dialogue bat replaces the plain one (DAT_004E1AC0, DAT_004E1A40) once the progress bit is set.
    const auto gold_bat = [&s]()
    {
        if ((s.profile.progress.bytes[4] & 0x40) != 0) // *(DAT_00506C6C + 8) + 4
        {
            set_cutscene_actor_replacement(s.replacement_source, s.replacement_target,
                "aid_model_ghoulies_actor_dialogbat", "aid_model_ghoulies_actor_dialogbatgold"); // 0x0004E610
        }
    };
    char page[0x80]{};
    switch (kind) // The jump table at 0x000D44C0.
    {
    case 1:
        std::strcpy(page, "aid_cutscene_ghoulies_dialogs_dialogpage1"); // 0x00304E30
        break;
    case 2:
        std::strcpy(page, "aid_cutscene_ghoulies_dialogs_reallysad"); // 0x00304CB0
        break;
    case 3:
        std::strcpy(page, "aid_cutscene_ghoulies_dialogs_reallyaverage"); // 0x00304D30
        break;
    case 4:
        std::strcpy(page, "aid_cutscene_ghoulies_dialogs_reallyhappy"); // 0x00304DB0
        break;
    default:
        std::strcpy(page, "aid_cutscene_ghoulies_dialogs_dialogpage1"); // 0x00304E30
        gold_bat(); // 0x000D43A2: the default case's own copy of the swap.
        break;
    }
    gold_bat(); // 0x000D4408
    state.cutscene = load_runtime_cutscene(page, 6, &state.camera, s.cutscenes); // 0x0004D680, +0x10C
}

// Xbox 0x000D3450
void start_runtime_final_score_intro(FinalScoreServices& services)
{
    auto& s = services.frontend;
    services.intro = std::make_unique<i32>(0); // 0x000F72D0(4) into DAT_00506560
    set_runtime_final_score_cutscene(0, 0, services); // 0x000D4280
    // The tail at 0x000D3510 with EAX 1.
    *services.intro = 1;
    s.random = s.random * 0x19660Du + 0x3C6EF35Fu; // DAT_0054DD7C
    auto* dialogue = s.cutscenes.dialogue;
    if (dialogue == nullptr)
    {
        throw Error("0x000D3450: the scene dialogue (0x000582E0, DAT_004EE1E8) is not bound");
    }
    if (i32(dialogue->state.count) + 1 < 9) // DAT_004EE1E8
    {
        const float unit = std::bit_cast<float>((s.random & 0x7FFFFFu) | 0x3F800000u);
        const float one = std::bit_cast<float>(read_u32(s.image.memory(0x0032B0F8, 4), 0));
        const float scale = std::bit_cast<float>(read_u32(s.image.memory(0x0032B664, 4), 0)); // -6
        const i32 index = i32((unit - one) * scale); // CVTTSS2SI: 0..-5
        const char* key = s.image.string(read_u32(s.image.memory(0x00428B34u - u32(index) * 4u, 4), 0));
        request_namespaced_localtext("aid_loctext_ghoulies_dialog_dialogs", "dialogs", key,
            dialogue->measurement.wide_scratch, dialogue->measurement.localtext); // 0x000FF0A0 into DAT_00507E78
        enqueue_runtime_dialogue(dialogue->measurement.wide_scratch, 0, *dialogue); // 0x000582E0
    }
}

// Xbox 0x000D3480
void free_runtime_final_score_intro(FinalScoreServices& services)
{
    services.intro.reset(); // HeapFree(DAT_00506560)
}

// Xbox 0x000D34D0
void tick_runtime_final_score_intro(FinalScoreServices& services)
{
    if (services.intro == nullptr)
    {
        throw Error("0x000D34D0 reads the final score intro (DAT_00506560) before 0x000D3450 makes it");
    }
    if (*services.intro == 1 && services.frontend.dialogue_page_hold() == 0) // DAT_004EE1EC
    {
        *services.intro = 2;
    }
}

// Xbox 0x000D34F0
bool runtime_final_score_intro_done(FinalScoreServices& services)
{
    if (services.intro == nullptr)
    {
        return true;
    }
    return *services.intro == 2;
}

// Xbox 0x000D4190
void open_runtime_final_score_intro(const char16_t* name, FinalScoreServices& services)
{
    auto& state = state_of(services);
    if (name != nullptr && name[0] != 0)
    {
        std::char_traits<char16_t>::copy(state.name.data(), name,
            std::char_traits<char16_t>::length(name) + 1); // lstrcpyW(+4, EAX)
    }
    build_runtime_final_score_tally(services); // 0x000D2880
    free_current_page(state, services);
    state.page = 1;
    page_of(1).start(services); // 0x000D3450, inline
}

// Xbox 0x000D41E0
void open_runtime_final_score_scores(const char16_t* name, FinalScoreServices& services)
{
    auto& state = state_of(services);
    if (name != nullptr && name[0] != 0)
    {
        std::char_traits<char16_t>::copy(state.name.data(), name,
            std::char_traits<char16_t>::length(name) + 1); // lstrcpyW(+4, EAX)
    }
    build_runtime_final_score_tally(services); // 0x000D2880
    free_current_page(state, services);
    state.page = 2;
    page_of(2).start(services); // JMP [0x00304ED8], 0x000D1540
}

// Xbox 0x000D24B0
void lay_out_runtime_final_score_row(i32 index, FinalScoreServices& services)
{
    auto& s = services.frontend;
    auto& page = scores_of(services);
    auto& row = page.rows[size_t(index)];
    const auto& tally = tally_of(services).rows[size_t(index)];
    const float cx = s.hud.center_x; // DAT_00509730
    const float cy = s.hud.center_y; // DAT_00509734
    const float half = exe_real(s, 0x0032B0F4);
    lookup_borrowed_localtext(state_of(services).text, "finalscore", tally.name.data(), row.label.text.data(),
        s.localtext); // 0x000FF120, ESI the score text, EDI the tally row's name, EBX +4
    std::char_traits<char16_t>::copy(row.value.text.data(), tally.value.data(),
        std::char_traits<char16_t>::length(tally.value.data()) + 1); // lstrcpyW(+0x228, the tally's +0x188)
    const float small = exe_real(s, 0x00428A88);
    const float label_width = runtime_text_line_width(s.metrics, 0, row.label.text.data(), small); // 0x00060710
    float width = runtime_text_line_width(s.metrics, 0, row.value.text.data(), small) + exe_real(s, 0x00428ACC)
        + label_width; // 0x00060710
    if (width > s.hud.safe_width) // DAT_0050971C
    {
        width = s.hud.safe_width;
    }
    const float left = cx - width * half;
    const float top = exe_real(s, 0x00428AC8) + cy;
    row.label.x = left; // +0x204
    row.label.y = top; // +0x208
    row.value.y = top; // +0x42C
    row.value.x = left + label_width; // +0x428
    format_wide_integer(tally.rank, row.rank.text.data(), 10); // 0x00166B36, +0x434
    format_wide_integer(tally.rank_count, row.rank_count.text.data(), 10); // 0x00166B36, +0x640
    const float large = exe_real(s, 0x00428A8C);
    const float rank_width = runtime_text_line_width(s.metrics, 0, row.rank.text.data(), large); // 0x00060710
    const float count_width = runtime_text_line_width(s.metrics, 0, row.rank_count.text.data(), large);
    const float slash_width = runtime_text_line_width(s.metrics, 0, page.slash.text.data(), large) * half;
    const float gap = exe_real(s, 0x00428AD0);
    const float lower = cy + exe_real(s, 0x00428AD4);
    row.rank.y = cy; // +0x638
    row.rank.x = cx; // +0x634
    row.rank.y = lower;
    row.rank.x = row.rank.x - (rank_width * half + slash_width + gap);
    row.rank_count.y = lower; // +0x844
    row.rank_count.x = count_width * half + slash_width + gap + cx; // +0x840
    std::span<const u8> icon;
    if (std::memcmp(tally.icon.data(), "aid_", 4) == 0) // *(+0x100) == 0x5F646961
    {
        icon = acquire_animation_resource(s.resources, tally.icon.data()); // 0x00035E90
        if (icon.data() == nullptr)
        {
            resource_error(s.error, 2); // 0x00032700(2)
        }
    }
    row.icon = icon; // +0x210
    row.unk_218 = read_u32(s.image.memory(0x00314F48, 4), 0); // +0x21C
    const float scale = exe_real(s, 0x00428A98 + u32(index) * 4);
    row.icon_scale = {scale, scale}; // +0x220, +0x224
}

// Xbox 0x000D1DC0
void lay_out_runtime_final_score_rows(FinalScoreServices& services)
{
    auto& s = services.frontend;
    auto& page = scores_of(services);
    const float cx = s.hud.center_x; // DAT_00509730
    const float cy = s.hud.center_y; // DAT_00509734
    for (i32 index = 0; index < 4; ++index)
    {
        lay_out_runtime_final_score_row(index, services); // 0x000D24B0
    }
    auto& rows = page.rows;
    const float half = exe_real(s, 0x0032B0F4);
    const float gap = exe_real(s, 0x00428A94);
    const float widths = icon_width(rows[0]) * rows[0].icon_scale[0] + icon_width(rows[1]) * rows[1].icon_scale[0]
        + icon_width(rows[2]) * rows[2].icon_scale[0] + icon_width(rows[3]) * rows[3].icon_scale[0];
    const float left = cx - (gap * exe_real(s, 0x0032B260) + widths) * half;
    // Each icon's own width (its +0x224 scale) sets its step: steps 0, 1, 2 (DAT_0032B110) and 3 (DAT_0032B260)
    // of the gap and width, then half the width.
    const float steps[4]{exe_real(s, 0x0032B100), 1.f, exe_real(s, 0x0032B110), exe_real(s, 0x0032B260)};
    for (size_t index = 0; index < 4; ++index)
    {
        auto& row = rows[index];
        const float width = icon_width(row) * row.icon_scale[1];
        row.icon_position = {cx, cy};
        row.icon_position[0] = (gap + width) * steps[index] + width * half + left;
    }
}

// Xbox 0x000D2730
void fly_runtime_final_score_bat(FinalScoreServices& services)
{
    auto& s = services.frontend;
    auto& page = scores_of(services);
    const auto& row = page.rows[size_t(page.row)];
    ButtonBatPoint point{row.icon_position[0],
        row.icon_position[1] - icon_height(row) * row.icon_scale[1] * exe_real(s, 0x0032B0F4)};
    if (page.bat == nullptr)
    {
        throw Error("0x000D2730 flies final score page 2's bat (+0x2134) before it exists");
    }
    s.fly_bat(*page.bat, point, 0.f); // 0x0006DA00, ESI the bat, EBX the point
    page.bat_point = point; // +0x2138
}

// Xbox 0x000D27D0
i16 start_runtime_final_score_loop(const char* name, FinalScoreServices& services)
{
    auto& voices = services.frontend.voices;
    const i16 voice = allocate_audio_event_voice(0, voices); // 0x00039830, EBX 0
    if (voice != -1)
    {
        voices.voices[size_t(voice)].stop = 0; // DAT_004E0CEC + voice * 0x24
        prepare_audio_event_voice(voice, name, voices); // 0x00039A20, EAX the voice
    }
    return voice;
}

// Xbox 0x000D2810
void stop_runtime_final_score_loop(i16 voice, FinalScoreServices& services)
{
    if (voice != -1)
    {
        stop_audio_event_voice(voice, 0, services.frontend.voices); // 0x00039690
    }
}

// Xbox 0x000D4570
const char* runtime_final_score_rank_line(u16 rank, u16 low, u16 high, FinalScoreServices& services)
{
    const auto& s = services.frontend;
    const float one = exe_real(s, 0x0032B0F8);
    const float last = exe_real(s, 0x0032B1B0); // 4
    if (one == float(i32(rank) - i32(low)) / float(i32(high) - i32(low)))
    {
        return s.image.string(0x00304F28u + 3u * 0x80u); // XACT_SOUNDBANK_GBOY_21TO25
    }
    float index = last;
    if (float(i32(high)) != float(i32(low)))
    {
        index = (float(i32(rank)) - float(i32(low))) / (float(i32(high)) - float(i32(low))) * last;
        if (last < index)
        {
            index = last;
        }
        else if (index < 0.f)
        {
            index = 0.f;
        }
    }
    return s.image.string(0x00304F28u + u32(i32(index)) * 0x80u); // XACT_SOUNDBANK_GBOY_0TO6, stride 0x80
}

// Xbox 0x000D1FC0
void set_runtime_final_score_scores_state(i32 next, FinalScoreServices& services)
{
    auto& s = services.frontend;
    auto& page = scores_of(services);
    if (page.state == 3)
    {
        stop_voice(page.snare, s);
    }
    page.state = next;
    auto& row = page.rows[size_t(page.row)];
    const auto& tally = tally_of(services).rows[size_t(page.row)];
    switch (next) // The jump table at 0x000D248C.
    {
    case 2:
        fly_runtime_final_score_bat(services); // 0x000D2730
        if (row.voice == -1)
        {
            row.voice = start_runtime_final_score_loop(tally.sound, services); // 0x000D27D0
        }
        return;
    case 3:
        row.label.record = add_line(page, row.label, exe_real(s, 0x00428A88), 0, s); // 0x0005F580, EAX 0
        request_audio_event_voice("XACT_SOUNDBANK_GPICKUPDITTIES_EVENTREQICON", 1, s.voices); // 0x000395E0
        if (page.snare == -1)
        {
            page.snare = start_runtime_final_score_loop("XACT_SOUNDBANK_GPROPS_SNAREROLL", services); // 0x000D27D0
        }
        page.timer = exe_real(s, 0x00428AE0);
        return;
    case 4:
        row.value.record = add_line(page, row.value, exe_real(s, 0x00428A88), 0, s); // 0x0005F580, EAX 0
        request_audio_event_voice("XACT_SOUNDBANK_GSTREAMSDITTIES_SCOREFANFARE", 1, s.voices); // 0x000395E0
        page.timer = exe_real(s, 0x00428AE4);
        return;
    case 5:
    {
        const float large = exe_real(s, 0x00428A8C);
        row.rank.record = add_line(page, row.rank, large, 1, s); // 0x0005F580, EAX 1
        page.slash.record = add_line(page, page.slash, large, 1, s);
        row.rank_count.record = add_line(page, row.rank_count, large, 1, s);
        request_audio_event_voice("XACT_SOUNDBANK_GPICKUPDITTIES_KEYAPPEAR", 1, s.voices); // 0x000395E0
        page.timer = exe_real(s, 0x00428AE8);
        return;
    }
    case 6:
        request_audio_event_voice(runtime_final_score_rank_line(tally.rank, 0, tally.rank_count, services), 1,
            s.voices); // 0x000D4570 (SI the rank, CX 0, DX the count), 0x000395E0
        page.timer = exe_real(s, 0x00428AEC);
        return;
    case 7:
    {
        page.score.text[0] = 0;
        lookup_borrowed_localtext(state_of(services).text, "finalscore", "score", page.score.text.data(),
            s.localtext); // 0x000FF120, EDI 0x00324918, EBX +0x257C
        page.score.x = s.hud.center_x; // +0x277C
        page.score.y = s.hud.center_y;
        page.score.y = exe_real(s, 0x00428AC4);
        page.total = page.total + tally.rank;
        char16_t digits[20]{};
        format_wide_integer(i32(page.total), digits, 10); // 0x00166B36
        std::char_traits<char16_t>::copy(page.score.text.data() + std::char_traits<char16_t>::length(
            page.score.text.data()), digits, std::char_traits<char16_t>::length(digits) + 1); // lstrcatW
        page.score.record = add_line(page, page.score, exe_real(s, 0x00428A90), 1, s); // 0x0005F580, EAX 1
        request_audio_event_voice("XACT_SOUNDBANK_GPICKUPDITTIES_REAPERICON", 1, s.voices); // 0x000395E0
        page.timer = exe_real(s, 0x00428AF0);
        return;
    }
    case 8:
        end_all_dialogue_text_records(*page.pool, s.fonts, s.fallback_texture, s.random); // 0x0005F970
        page.score.record = 0;
        page.slash.record = 0;
        row.rank_count.record = 0;
        row.rank.record = 0;
        row.value.record = 0;
        page.timer = exe_real(s, 0x00428AF4);
        row.label.record = 0;
        stop_runtime_final_score_loop(row.voice, services); // 0x000D2810, AX the row's voice
        row.voice = -1;
        return;
    case 9:
        fly_runtime_bat_off_screen(page.bat, &page.bat_point.x, s); // 0x0013B9D0, ECX the bat, EAX +0x2138
        return;
    default:
        return;
    }
}

// Xbox 0x000D1540
void start_runtime_final_score_scores(FinalScoreServices& services)
{
    auto& s = services.frontend;
    services.scores = std::make_unique<FinalScoreScoresPage>(); // 0x000F72D0(0x278C), its words cleared
    auto& page = *services.scores;
    for (auto& row : page.rows)
    {
        row.unk_218 = read_u32(s.image.memory(0x00314F48, 4), 0);
        row.voice = -1;
    }
    page.snare = -1;
    lookup_borrowed_localtext(state_of(services).text, "finalscore", "pagetitle", page.title.text.data(),
        s.localtext); // 0x000FF120, EDI 0x00324924, EBX +0x2140
    page.title.x = s.hud.center_x; // +0x2340
    page.title.y = s.hud.center_y;
    page.title.y = exe_real(s, 0x00428AC0);
    page.slash.x = s.hud.center_x; // +0x2570
    page.slash.y = s.hud.center_y;
    page.slash.y = page.slash.y + exe_real(s, 0x00428AD4);
    std::char_traits<char16_t>::copy(page.slash.text.data(), u"/", 2); // lstrcpyW(+0x2370, 0x00324920)
    allocate_dialogue_text_pool(page.pool, 0, 1, 0xC0, s.text_pools); // 0x0005F390, ESI +0x234C
    page.pool->origin = {exe_real(s, 0x0032B260), exe_real(s, 0x0032B264), std::bit_cast<float>(3u)};
    lay_out_runtime_final_score_rows(services); // 0x000D1DC0
    auto& rows = page.rows;
    page.panel_size[0] = (rows[3].icon_position[0] - rows[0].icon_position[0]) + icon_width(rows[0])
        + exe_real(s, 0x00428AA8) * exe_real(s, 0x0032B110); // +0x2360
    page.panel_size[1] = exe_real(s, 0x00428AB8); // +0x2364
    page.panel_color = read_u32(s.image.memory(0x00428ABC, 4), 0); // +0x2368
    page.panel_position = {s.hud.center_x, s.hud.center_y}; // +0x2358
    page.panel_position[0] = page.panel_position[0] + exe_real(s, 0x00428AB0);
    page.panel_position[1] = page.panel_position[1] + exe_real(s, 0x00428AB4);
    page.row = 0;
    page.bat = s.create_bat({0.f, 0.f}, 0.f, exe_real(s, 0x00428ADC)); // 0x0013B580(EAX {0, 0}, 0, 0.4)
    if (page.bat == nullptr)
    {
        throw Error("0x000D1540: 0x0013B580 made no dialogue bat");
    }
    const float scale = exe_real(s, 0x00428AD8);
    if (page.bat->scale_200 != scale) // +0x200
    {
        page.bat->scale_200 = scale;
        page.bat->xform_valid = 0; // +0x2A8
    }
    set_runtime_final_score_cutscene(1, 1, services); // 0x000D4280
    set_runtime_final_score_scores_state(2, services); // 0x000D1FC0, EAX 2
}

// Xbox 0x000D18B0
void free_runtime_final_score_scores(FinalScoreServices& services)
{
    auto& s = services.frontend;
    auto& page = scores_of(services);
    release_dialogue_text_pool(page.pool, *s.text_pools); // 0x0005F460
    page.pool = nullptr;
    for (auto& row : page.rows)
    {
        if (row.icon.data() != nullptr)
        {
            release_animation_resource(s.resources, row.icon); // 0x00035C00
        }
        stop_voice(row.voice, s);
    }
    stop_voice(page.snare, s);
    if (page.bat != nullptr)
    {
        activate_actor_bind_tree(*page.bat); // 0x00112D10, message 0x80 and 0x0003ED70: the 0x00112CC0 inline
        page.bat = nullptr;
    }
    services.scores.reset(); // HeapFree(DAT_00506554)
}

// Xbox 0x000D1A80
void tick_runtime_final_score_scores(FinalScoreServices& services)
{
    auto& s = services.frontend;
    auto& page = scores_of(services);
    const i32 run = services.run_state(); // DAT_00510F5C
    if ((run < 1 || 2 < run) && page.bat != nullptr)
    {
        s.update_actor(*page.bat); // Vtable +0x10.
    }
    switch (page.state) // The jump table at 0x000D1C50.
    {
    case 2:
        if (bat_arrived(page.bat, s))
        {
            set_runtime_final_score_scores_state(3, services); // 0x000D1FC0
        }
        return;
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
        if (s.timer_expired(&page.timer) != 0) // 0x0012B2A0, EAX +0x2350
        {
            set_runtime_final_score_scores_state(page.state + 1, services); // 0x000D1FC0
        }
        return;
    case 8:
        if (s.timer_expired(&page.timer) == 0) // 0x0012B2A0
        {
            return;
        }
        if (page.row < 3)
        {
            ++page.row;
            set_runtime_final_score_scores_state(2, services); // 0x000D1FC0
            return;
        }
        // 0x000D1FC0's state 9, inline.
        if (page.state == 3)
        {
            stop_voice(page.snare, s);
        }
        page.state = 9;
        fly_runtime_bat_off_screen(page.bat, &page.bat_point.x, s); // JMP 0x0013B9D0
        return;
    case 9:
        if (bat_arrived(page.bat, s))
        {
            if (page.state == 3)
            {
                stop_voice(page.snare, s);
            }
            page.state = 10;
        }
        return;
    default:
        return;
    }
}

// Xbox 0x000D1C70
void draw_runtime_final_score_scores(FinalScoreServices& services)
{
    auto& s = services.frontend;
    auto& page = scores_of(services);
    const float half = exe_real(s, 0x0032B0F4);
    draw_runtime_dialogue_panel(float(i32(page.panel_position[0]) - i32(page.panel_size[0] * half)),
        float(i32(page.panel_position[1]) - i32(page.panel_size[1] * half)), float(i32(page.panel_size[0])),
        float(i32(page.panel_size[1])), exe_real(s, 0x00428AAC), page.panel_color, s.outer); // 0x00061360
    const u32 color = read_u32(s.image.memory(0x00314F48, 4), 0);
    for (auto& row : page.rows)
    {
        if (row.icon.data() == nullptr)
        {
            continue;
        }
        auto& draw = s.draw;
        begin_dialogue_sprite_draw(draw); // 0x0005CDC0
        draw_dialogue_texture(-1, nullptr, const_cast<u8*>(row.icon.data()), row.icon_scale.data(), nullptr, 0.f,
            color, row.icon_position.data(), draw); // 0x0005CA60, ECX -1, EDX 0, EAX +0x214
        --draw.override_count; // DAT_00509B5C
        if (draw.override_count == 0)
        {
            draw.fog_disabled = 0; // DAT_00509B00
        }
    }
    if (page.bat != nullptr)
    {
        s.draw_actor(*page.bat, i32(s.outer.current_display_address - 0x507018u) / 0x1E4, 3); // Vtable +0x14
    }
}

// Xbox 0x000D1DA0
bool runtime_final_score_scores_done(FinalScoreServices& services)
{
    if (services.scores == nullptr)
    {
        return true;
    }
    return services.scores->state == 10;
}

FinalScoreSuperScaryPage::~FinalScoreSuperScaryPage() = default;

namespace
{
FinalScoreSuperScaryPage& super_scary_of(FinalScoreServices& services)
{
    if (services.super_scary == nullptr)
    {
        throw Error("final score page 3 (DAT_0050656C) is read before 0x000D4610 makes it");
    }
    return *services.super_scary;
}
}

// Xbox 0x000D4230
void open_runtime_final_score_super_scary(const char16_t* name, FinalScoreServices& services)
{
    auto& state = state_of(services);
    if (name != nullptr && name[0] != 0)
    {
        std::char_traits<char16_t>::copy(state.name.data(), name,
            std::char_traits<char16_t>::length(name) + 1); // lstrcpyW(+4, EAX)
    }
    build_runtime_final_score_tally(services); // 0x000D2880
    free_current_page(state, services);
    state.page = 3;
    page_of(3).start(services); // JMP [0x00304EEC], 0x000D4610
}

// Xbox 0x000D4BE0
void set_runtime_final_score_super_scary_state(i32 next, FinalScoreServices& services)
{
    auto& s = services.frontend;
    auto& page = super_scary_of(services);
    const auto& tally = tally_of(services);
    page.state = next;
    switch (next) // The jump table at 0x000D4E54.
    {
    case 1:
        page.title.record = create_dialogue_text_record(*page.pool, page.title.text.data(), page.title.x,
            page.title.y, exe_real(s, 0x00428BB0), exe_real(s, 0x00428BB0), 1.f, 1, 0, 1, s.metrics,
            s.viewport_width, s.next_text_id, s.random); // 0x0005F5C0, EAX 1
        page.score.record = create_dialogue_text_record(*page.pool, page.score.text.data(), page.score.x,
            page.score.y, exe_real(s, 0x00428BB4), exe_real(s, 0x00428BB4), 1.f, 1, 0, 1, s.metrics,
            s.viewport_width, s.next_text_id, s.random); // 0x0005F5C0, EAX 1
        return;
    case 2:
    {
        // The challenge ditties from 0x00305438 up (winnowt, bronze, silver, gold, platinum): the total times
        // DAT_0032B5CC (about -1/20), truncated, steps back by 0x80.
        const i32 step = i32(float(u16(tally.total)) * exe_real(s, 0x0032B5CC));
        request_audio_event_voice(s.image.string(u32(0x00305438 - step * 0x80)), 1, s.voices); // 0x000395E0
        page.timer = exe_real(s, 0x00428BC0);
        return;
    }
    case 3:
        request_audio_event_voice(runtime_final_score_rank_line(u16(tally.total), 0, 100, services), 1,
            s.voices); // 0x000D4570 (SI the total, CX 0, DX 100), 0x000395E0
        page.timer = exe_real(s, 0x00428BC4);
        return;
    case 4:
        if (u16(tally.save_19c) != 0)
        {
            if (page.brew != nullptr)
            {
                page.brew->state = 2;
                page.brew->timer = page.brew->duration - page.brew->timer;
            }
            format_wide_integer(u16(tally.save_19c), page.brew_count.text.data(), 10); // 0x00166B36, +0x428
            page.brew_count.x = s.hud.center_x; // +0x628
            page.brew_count.y = s.hud.center_y;
            page.brew_count.x = page.brew_count.x + exe_real(s, 0x00428BF8);
            page.brew_count.y = page.brew_count.y + exe_real(s, 0x00428BFC);
            page.brew_count.x = page.brew_count.x + exe_real(s, 0x00428C00);
            page.brew_count.y = page.brew_count.y + exe_real(s, 0x00428C04);
            page.brew_count.record = create_dialogue_text_record(*page.pool, page.brew_count.text.data(),
                page.brew_count.x, page.brew_count.y, exe_real(s, 0x00428BEC), exe_real(s, 0x00428BEC), 1.f, 0, 0,
                1, s.metrics, s.viewport_width, s.next_text_id, s.random); // 0x0005F580, EAX 0
            request_audio_event_voice("XACT_SOUNDBANK_GFINALSCORE_BUTLERSBREW", 1, s.voices); // 0x000395E0
        }
        page.timer = exe_real(s, 0x00428BC8);
        return;
    case 5:
        if (page.prompts == nullptr)
        {
            throw Error("0x000D4BE0's state 5 opens final score page 3's prompts (+0x634) before they exist");
        }
        set_dialogue_widget_prompt(*page.prompts, s.image.memory(0x00305128, 0x310).data(),
            s.widgets); // JMP 0x00142180, ECX +0x634, EAX 0x00305128
        return;
    default:
        return;
    }
}

// Xbox 0x000D4610
void start_runtime_final_score_super_scary(FinalScoreServices& services)
{
    auto& s = services.frontend;
    services.super_scary = std::make_unique<FinalScoreSuperScaryPage>(); // 0x000F72D0(0x638), its words cleared
    auto& page = *services.super_scary;
    const auto& tally = tally_of(services);
    lookup_borrowed_localtext(state_of(services).text, "finalscore", "superscaryscore", page.title.text.data(),
        s.localtext); // 0x000FF120, EDI 0x003249FC, EBX +4
    page.title.x = s.hud.center_x; // +0x204
    page.title.y = s.hud.center_y;
    page.title.y = exe_real(s, 0x00428BB8);
    format_wide_integer(u16(tally.total), page.score.text.data(), 10); // 0x00166B36, ESI +0x210
    page.score.x = s.hud.center_x; // +0x410
    page.score.y = s.hud.center_y;
    page.score.y = exe_real(s, 0x00428BBC);
    char16_t hundred[20]{};
    format_wide_integer(100, hundred, 10); // 0x00166B36
    const auto append = [&page](const char16_t* text)
    {
        auto* end = page.score.text.data() + std::char_traits<char16_t>::length(page.score.text.data());
        std::char_traits<char16_t>::copy(end, text, std::char_traits<char16_t>::length(text) + 1); // lstrcatW
    };
    append(u" / "); // 0x003249B8
    append(hundred);
    allocate_dialogue_text_pool(page.pool, 0, 1, 0xC0, s.text_pools); // 0x0005F390, ESI +0x41C
    page.pool->origin = {exe_real(s, 0x0032B260), exe_real(s, 0x0032B264), std::bit_cast<float>(3u)};
    const float icon_scale = exe_real(s, 0x00428BDC);
    const float spacing = exe_real(s, 0x00428BD4);
    const float lift = spacing * exe_real(s, 0x0032B0F4) * icon_scale;
    const std::array<float, 2> anchor{s.hud.center_x + exe_real(s, 0x00428BCC),
        s.hud.inner_bottom - exe_real(s, 0x00428BD0)}; // DAT_0050974C
    const std::array<float, 2> row_step{icon_scale * 0.f, 0.f - icon_scale * spacing};
    const std::array<float, 2> origin{lift + exe_real(s, 0x00428BD8), 0.f - lift};
    create_dialogue_widget(page.prompts, 0, nullptr, anchor, row_step, origin, nullptr, icon_scale,
        exe_real(s, 0x00428BE0), "finalscore", state_of(services).text, 0x400C0,
        s.widget_allocation); // 0x00141E20, EAX +0x634
    auto& prompts = *page.prompts;
    const float fade = exe_real(s, 0x0032B1AC);
    prompts.duration = fade;
    if (prompts.progress > fade)
    {
        prompts.progress = fade;
    }
    prompts.pool->end_time = fade;
    prompts.pool->origin = {exe_real(s, 0x0032B260), exe_real(s, 0x0032B264), std::bit_cast<float>(3u)};
    const std::array<float, 2> position{exe_real(s, 0x00428BF8) + s.hud.center_x,
        exe_real(s, 0x00428BFC) + s.hud.center_y};
    const std::array<float, 2> full_scale{exe_real(s, 0x00428BF0), exe_real(s, 0x00428BF4)};
    page.brew = std::make_unique<ButtonBatMenuSprite>(); // 0x000F72D0(0x2C)
    initialize_menu_sprite(*page.brew, "aid_texture_ghoulies_scoreboard_energy_butlersbrew", position, full_scale,
        exe_real(s, 0x00428C08), s); // 0x00144780, EAX 0x003056B8, EBX 0x00428BF0
    const i32 total = i32(u16(tally.total));
    i32 kind = 4; // reallyhappy
    if (total < exe_int(s, 0x00428BE4)) // 66
    {
        kind = (exe_int(s, 0x00428BE8) <= total ? 1 : 0) + 2; // 33: reallyaverage, else reallysad
    }
    set_runtime_final_score_cutscene(1, kind, services); // 0x000D4280
    set_runtime_final_score_super_scary_state(1, services); // 0x000D4BE0, EAX 1
}

// Xbox 0x000D4970
void free_runtime_final_score_super_scary(FinalScoreServices& services)
{
    auto& s = services.frontend;
    auto& page = super_scary_of(services);
    if (page.prompts != nullptr)
    {
        destroy_dialogue_widget(page.prompts, s.resources, *s.text_pools); // 0x00142070
        page.prompts = nullptr;
    }
    if (page.brew != nullptr)
    {
        release_menu_sprite(page.brew, s); // 0x00144820, EAX +0x424
    }
    release_dialogue_text_pool(page.pool, *s.text_pools); // 0x0005F460, EBX +0x41C
    page.pool = nullptr;
    services.super_scary.reset(); // HeapFree(DAT_0050656C)
}

// Xbox 0x000D4A00
void tick_runtime_final_score_super_scary(FinalScoreServices& services)
{
    auto& s = services.frontend;
    auto& page = super_scary_of(services);
    if (page.prompts != nullptr && s.dialogue_page_hold() == 0) // DAT_004EE1EC
    {
        s.update_widget(*page.prompts, services.input_address); // 0x00142460, EAX the state's +0x170 device
    }
    switch (page.state) // The jump table at 0x000D4B28.
    {
    case 1:
        if (dialogue_text_record_animating(*page.pool, page.title.record, s.fonts, s.fallback_texture) == 0)
        {
            set_runtime_final_score_super_scary_state(2, services); // 0x000D4BE0, EAX 2
        }
        break;
    case 2:
        if (s.timer_expired(&page.timer) != 0) // 0x0012B2A0, EAX +0x420
        {
            // 0x000D4BE0's state 3, inline.
            page.state = 3;
            request_audio_event_voice(runtime_final_score_rank_line(u16(tally_of(services).total), 0, 100,
                services), 1, s.voices); // 0x000D4570 (SI the total, CX 0, DX 100), 0x000395E0
            page.timer = exe_real(s, 0x00428BC4);
        }
        break;
    case 3:
        if (s.timer_expired(&page.timer) != 0)
        {
            set_runtime_final_score_super_scary_state(4, services); // 0x000D4BE0, EAX 4
        }
        break;
    case 4:
        if (s.timer_expired(&page.timer) != 0)
        {
            // 0x000D4BE0's state 5, inline.
            page.state = 5;
            set_dialogue_widget_prompt(*page.prompts, s.image.memory(0x00305128, 0x310).data(),
                s.widgets); // 0x00142180, ECX +0x634, EAX 0x00305128
        }
        break;
    case 6:
        if (page.prompts->mode == 1)
        {
            page.state = 7;
        }
        break;
    default:
        break;
    }
    if (page.brew != nullptr)
    {
        tick_menu_sprite(*page.brew, s); // JMP 0x00144880, ECX +0x424
    }
}

// Xbox 0x000D4B40
void draw_runtime_final_score_super_scary(FinalScoreServices& services)
{
    auto& s = services.frontend;
    auto& page = super_scary_of(services);
    if (page.prompts != nullptr && s.dialogue_page_hold() == 0) // DAT_004EE1EC
    {
        draw_runtime_dialogue_widget(*page.prompts, 0, s.outer); // 0x00142720
    }
    auto* brew = page.brew.get();
    if (brew != nullptr && brew->texture.data() != nullptr && (brew->scale[0] != 0.f || brew->scale[1] != 0.f))
    {
        draw_runtime_dialogue_sprite(brew->texture.data(), brew->position.data(), brew->scale.data(), 0.f,
            brew->color, -1, s.draw); // 0x0005CE30
    }
}

// Xbox 0x000D4E70
void accept_runtime_final_score_super_scary(FinalScoreServices& services)
{
    auto& page = super_scary_of(services);
    page.state = 6;
    if (page.prompts == nullptr)
    {
        throw Error("0x000D4E70 reads final score page 3's prompts (+0x634) before they exist");
    }
    page.prompts->requested_set = nullptr; // +0x950
    set_dialogue_widget_mode(*page.prompts, 3, services.frontend.widgets); // 0x00142C20, EAX 3
}

// Xbox 0x000D4BC0
bool runtime_final_score_super_scary_done(FinalScoreServices& services)
{
    if (services.super_scary == nullptr)
    {
        return true;
    }
    return services.super_scary->state == 7;
}

// Xbox 0x000D4070
void tick_runtime_final_score(FinalScoreServices& services)
{
    auto& s = services.frontend;
    auto& state = state_of(services);
    if (s.dialogue_page_hold() == 0 && state.cutscene_on != 0 && state.cutscene != nullptr) // DAT_004EE1EC
    {
        if (state.cutscene->duration <= state.cutscene->time) // +0x34, +0x2C
        {
            seek_runtime_cutscene(*state.cutscene, 0.f, 0.f, 0, s.cutscenes); // 0x0004E4A0
        }
        tick_runtime_cutscene(*state.cutscene, frame_step(s), &state.camera, s.cutscenes); // 0x0004DA50
    }
    // The jump table at 0x00304EB8 + page * 0x14.
    if (0 < state.page && state.page < 6)
    {
        page_of(state.page).tick(services);
    }
}

// Xbox 0x000D4120
void draw_runtime_final_score(FinalScoreServices& services)
{
    auto& s = services.frontend;
    auto& state = state_of(services);
    if (s.dialogue_page_hold() == 0 && state.cutscene_on != 0 && state.cutscene != nullptr) // DAT_004EE1EC
    {
        setup_explicit_camera_display(&state.camera, 0, s.display); // 0x00048BC0, ESI +0x10C, EAX 0
        const i32 view = i32(s.outer.current_display_address - 0x507018u) / 0x1E4; // DAT_005077A8
        draw_runtime_cutscene(*state.cutscene, view, s.cutscenes); // 0x0004DBD0, EBX +0x108
    }
    // The jump table at 0x00304EBC + page * 0x14.
    if (0 < state.page && state.page < 6)
    {
        page_of(state.page).draw(services);
    }
}

// Xbox 0x00082DC0
bool construct_runtime_final_score_control(std::span<u8> control, FinalScoreServices& services)
{
    namespace field = final_score_control_field;
    auto& s = services.frontend;
    set(control, field::page, 0);
    ++s.pause_count; // DAT_00510CEC
    s.flags |= 0x100000u; // DAT_0050EA9C
    finish_runtime_game_save(s.profile, services.saves); // 0x000F2D70
    create_runtime_final_score(services); // 0x000D3E30
    set(control, field::page, 1);
    start_runtime_final_score_ditty(services); // 0x000D44D0
    const char16_t* name = s.profile.name[0] != 0 ? s.profile.name.data() : nullptr; // DAT_00506C90, or null
    open_runtime_final_score_intro(name, services); // 0x000D4190, EAX
    return true;
}

// Xbox 0x00082E20
void destroy_runtime_final_score_control(FinalScoreServices& services)
{
    auto& s = services.frontend;
    destroy_runtime_final_score(services); // 0x000D3FB0
    s.flags &= ~0x100000u; // DAT_0050EA9C
    --s.pause_count; // DAT_00510CEC
}

// Xbox 0x00082E50
void tick_runtime_final_score_control(std::span<u8> control, FinalScoreServices& services)
{
    namespace field = final_score_control_field;
    auto& s = services.frontend;
    const float time = read_f32(control, 0x20) + frame_step(s);
    std::memcpy(control.data() + 0x20, &time, 4);
    tick_runtime_final_score(services); // 0x000D4070
    bool done = false;
    switch (get(control, field::page))
    {
    case 1:
        if (page_of(1).done(services)) // 0x00304ED4
        {
            set(control, field::page, 2);
            const char16_t* name = s.profile.name[0] != 0 ? s.profile.name.data() : nullptr; // DAT_00506C90
            open_runtime_final_score_scores(name, services); // 0x000D41E0, EAX
            return;
        }
        return;
    case 2:
        if (page_of(2).done(services)) // 0x00304EE8
        {
            set(control, field::page, 3);
            const char16_t* name = s.profile.name[0] != 0 ? s.profile.name.data() : nullptr; // DAT_00506C90
            open_runtime_final_score_super_scary(name, services); // 0x000D4230, EAX
            return;
        }
        return;
    case 3:
        done = page_of(3).done(services); // 0x00304EFC
        break;
    case 4:
        done = page_of(4).done(services); // 0x00304F10
        break;
    default:
        return;
    }
    if (done)
    {
        set(control, field::page, 5);
        char script[0x80]{};
        std::strcpy(script, "aid_script_ghoulies_credits_final");
        stop_runtime_final_score_ditty(services); // 0x000D4500
        s.request_script(script, 1); // 0x0011EB30(1)
    }
}

namespace
{
FinalScoreServices& final_score_of(GameSelectControlServices& s)
{
    if (s.final_score == nullptr)
    {
        throw Error("game select's final score (row 19): the final score services are not bound");
    }
    return *s.final_score;
}
}

// Xbox 0x000D79A0
GameSelectWidgetHandle create_runtime_game_final_score(GameSelectControlServices& services)
{
    (void)services;
    return std::make_unique<GameFinalScoreWidget>(); // 0x000F72D0(8)
}

// Xbox 0x000D79B0
void open_runtime_game_final_score(std::span<u8> owner, GameSelectControlState& control, GameSelectWidget& widget,
    GameSelectControlServices& services)
{
    auto& s = services;
    auto& w = static_cast<GameFinalScoreWidget&>(widget);
    (void)owner;
    s.frontend.final_score_widget = &w; // DAT_00506584
    w.state = 0;
    w.owner = &control;
    auto& final_score = final_score_of(s);
    create_runtime_final_score(final_score); // 0x000D3E30
    w.state = 1;
    open_runtime_final_score_super_scary(s.frontend.selected_name.data(), final_score); // 0x000D4230, EAX DAT_00429CC4
}

// Xbox 0x000D79F0
void close_runtime_game_final_score(GameSelectWidget& widget, GameSelectControlServices& services)
{
    (void)widget;
    destroy_runtime_final_score(final_score_of(services)); // 0x000D3FB0
    services.frontend.final_score_widget = nullptr; // DAT_00506584
}

// Xbox 0x000D7A00
void tick_runtime_game_final_score(GameSelectWidget& widget, GameSelectControlServices& services)
{
    auto& s = services;
    auto& w = static_cast<GameFinalScoreWidget&>(widget);
    if (w.state != 1)
    {
        return;
    }
    auto& final_score = final_score_of(s);
    tick_runtime_final_score(final_score); // 0x000D4070
    if (runtime_final_score_super_scary_done(final_score)) // (*0x00304EFC)(), 0x000D4BC0
    {
        auto& control = *find_runtime_scene_control(0x9A)->gameselect; // 0x00116170
        control.requested_row = 3;
        control.book_turn = 1;
        if (control.book != nullptr)
        {
            release_transition_book(control.book, s.books); // 0x00048860
            control.book = nullptr;
        }
        s.frontend.final_score_widget->state = 2; // *DAT_00506584
    }
}

// Xbox 0x00082F70: the final score's draw (a jump to 0x000D4120).
void draw_runtime_game_final_score(GameSelectWidget& widget, GameSelectControlServices& services)
{
    (void)widget;
    draw_runtime_final_score(final_score_of(services));
}
}
