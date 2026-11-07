#pragma once
#include "../scene/scene_control_fields.h"
#include "gbtg/entity.h"
#include "gbtg/object_factory.h"
#include "gbtg/retail_field.h"
#include "gameselect_rows.h"

#include <array>
#include <functional>
#include <memory>
#include <span>

namespace gbtg
{
struct GameSelectControlServices;
struct RuntimeCutsceneInstance;
struct RuntimeDialogueTextPool;
struct RuntimeDialogueWidget;
struct ButtonBatMenuSprite;
struct SaveGameContext;
// entitySceneControlFinalScore (factory type 0x99, vtable 0x005055F0): the end of the game. Its construct stores the
// finished game in its save (0x000F2D70), builds the final score (0x000D3E30) and opens page 1, the Baron's intro
// line. The update runs the current page (0x000D4070) and moves on through the pages as each one finishes: 2 the
// four scores counted up, 3 and 4, then 5 and aid_script_ghoulies_credits_final.
struct FinalScoreControlState
{
    SceneControlBase base;
    i32 page = 0; // +0x24, 0 building, then pages 1..4 and 5 the credits requested
};
static_assert(sizeof(FinalScoreControlState) == 0x28);
// One of the four scores (DAT_00506558 +r*0x388): its row of the 0x00303F60 table (stride 0x110) and its rank.
struct FinalScoreRow
{
    std::array<char, 0x100> name{}; // +0, the localtext key: timetaken, ghoulieskilled, timesfainted, prisonersrescued
    std::array<char, 0x80> icon{}; // +0x100, the scoreboard pause icon's AID
    const char* sound = nullptr; // +0x180, the row's XACT_SOUNDBANK_GFINALSCORE_ cue (0x003045A0, stride 0x80)
    u16 rank = 0; // +0x184, 1..25 against the row's 25 thresholds
    u16 rank_count = 0; // +0x186, the table's +0x108 (25 for every row)
    std::array<char16_t, 0x100> value{}; // +0x188, the score as text
};
// DAT_00506558, 0xE28 bytes (0x000F72D0).
struct FinalScoreTally
{
    std::array<FinalScoreRow, 4> rows{}; // +0
    u32 total = 0; // +0xE20, the four ranks summed
    u32 save_19c = 0; // +0xE24, the save's +0x19C word (the fifth time, DAT_00506C88)
};
// DAT_00506568, 0x1FC bytes (0x000F72D0).
struct FinalScoreState
{
    std::span<const u8> text; // +0, aid_loctext_ghoulies_finalscore_finalscoretext
    std::array<char16_t, 0x80> name{}; // +4, the finished game's save name
    i32 cutscene_on = 0; // +0x104, 0x000D4280's first argument
    RuntimeCutsceneInstance* cutscene = nullptr; // +0x108, the page's dialogue cutscene
    EntityCamNode camera{}; // +0x10C, the "finalscore" view
    i32 page = 0; // +0x16C, 1..5
    std::span<u8> input; // +0x170, the keyboard device 0x00107EF0 makes
    std::array<char, 0x80> ditty{}; // +0x174, XACT_SOUNDBANK_GSTREAMSDITTIES_PAUSE
    u32 ditty_playing = 0; // +0x1F4
    i32 cutscene_kind = 0; // +0x1F8, 0x000D4280's second argument
};
// A line of page 2's text: the text, its position and its text record (0x0005F580).
struct FinalScoreTextLine
{
    std::array<char16_t, 0x100> text{}; // +0
    float x = 0.f; // +0x200
    float y = 0.f; // +0x204
    u32 record = 0; // +0x208
};
// One score on page 2 (DAT_00506554 + 4 + r * 0x84C).
struct FinalScoreScoreRow
{
    FinalScoreTextLine label; // +0, the row's localtext ("finalscore", the tally row's name)
    std::span<const u8> icon; // +0x20C, the row's pause icon texture
    std::array<float, 2> icon_position{}; // +0x210
    u32 unk_218 = 0; // +0x218, DAT_00314F48
    std::array<float, 2> icon_scale{}; // +0x21C, DAT_00428A98 + r * 4
    FinalScoreTextLine value; // +0x224, the tally row's value
    FinalScoreTextLine rank; // +0x430, the rank
    FinalScoreTextLine rank_count; // +0x63C, the tally's +0x186
    i16 voice = -1; // +0x848, the row's looping cue (the tally row's sound)
};
// Page 2, the four scores (DAT_00506554, 0x278C bytes). State 2 flies the bat to a row's icon; 3..7 show its label
// (the snare roll), value, rank, the rank's voice line and the running total; 8 clears them and moves to the next
// row; 9 flies the bat away and 10 ends the page.
struct FinalScoreScoresPage
{
    i32 state = 0; // +0
    std::array<FinalScoreScoreRow, 4> rows{}; // +4
    Actor* bat = nullptr; // +0x2134, the dialogue button bat
    ButtonBatPoint bat_point{}; // +0x2138, the icon the bat last flew to
    FinalScoreTextLine title; // +0x2140, "pagetitle"
    RuntimeDialogueTextPool* pool = nullptr; // +0x234C
    float timer = 0.f; // +0x2350
    i32 row = 0; // +0x2354
    std::array<float, 2> panel_position{}; // +0x2358
    std::array<float, 2> panel_size{}; // +0x2360
    u32 panel_color = 0; // +0x2368
    u32 total = 0; // +0x236C, the ranks shown so far
    FinalScoreTextLine slash; // +0x2370, "/"
    FinalScoreTextLine score; // +0x257C, "score" and the total
    i16 snare = -1; // +0x2788, XACT_SOUNDBANK_GPROPS_SNAREROLL
};
// Page 3, the Super Scary Score (DAT_0050656C, 0x638 bytes). State 1 writes the title and "total / 100"; 2 plays the
// challenge ditty for the total (winnowt .. platinum); 3 the Baron's line for it; 4 the butler's brew bonus (the
// save's +0x19C) with its icon; 5 shows the prompt and 7 ends the page once it is taken (6 waits on it).
struct FinalScoreSuperScaryPage
{
    i32 state = 0; // +0
    FinalScoreTextLine title; // +4, "superscaryscore"
    FinalScoreTextLine score; // +0x210, the total, " / " and 100
    RuntimeDialogueTextPool* pool = nullptr; // +0x41C
    float timer = 0.f; // +0x420
    std::unique_ptr<ButtonBatMenuSprite> brew; // +0x424, the butler's brew icon (0x00144780, 0x2C bytes)
    FinalScoreTextLine brew_count; // +0x428
    RuntimeDialogueWidget* prompts = nullptr; // +0x634
    ~FinalScoreSuperScaryPage();
};
// Page 4, the end (DAT_0050655C, 0x234 bytes). State 1 writes the "theend_banner" text; 2 waits DAT_00428B18 once it
// has drawn; 3 waits for any button; 4 ends the text and 5 ends the page once the pool is empty. The "theend"
// texture is drawn throughout.
struct FinalScoreEndPage
{
    i32 state = 0; // +0
    FinalScoreTextLine banner; // +4, "theend_banner"
    RuntimeDialogueTextPool* pool = nullptr; // +0x210
    float timer = 0.f; // +0x214
    u32 unk_218 = 0; // +0x218
    std::span<const u8> texture{}; // +0x21C, aid_texture_ghoulies_finalscore_theend
    std::array<float, 2> position{}; // +0x220
    u32 color = 0; // +0x228, DAT_00314F48
    std::array<float, 2> scale{}; // +0x22C
};
// One of page 5's four scores (DAT_00506564 + 4 + r * 0x430): the row's icon and its value and rank as text.
struct FinalScoreSummaryEntry
{
    std::unique_ptr<ButtonBatMenuSprite> icon; // +0, the row's pause icon (0x00144780, 0x2C bytes)
    std::array<float, 3> unk_4{}; // +4
    FinalScoreTextLine value; // +0x10, the row's value (the tally's +0x188)
    float value_width = 0.f; // +0x21C, 0x00060710 at DAT_00428BA8
    FinalScoreTextLine rank; // +0x220, "rank / count"
    float rank_width = 0.f; // +0x42C, 0x00060710 at DAT_00428B50
};
// Page 5, the summary (DAT_00506564, 0x1504 bytes): the four rows' icons grow in on a panel; state 2 once all four
// are shown. Nothing in the XBE moves it to 3, its done state.
struct FinalScoreSummaryPage
{
    i32 state = 0; // +0
    std::array<FinalScoreSummaryEntry, 4> entries{}; // +4
    RuntimeDialogueTextPool* pool = nullptr; // +0x10C4
    FinalScoreTextLine title; // +0x10C8, "summary_pagetitle"
    std::array<float, 2> panel_position{}; // +0x12D4
    std::array<float, 2> panel_size{}; // +0x12DC
    u32 panel_color = 0; // +0x12E4, DAT_00428B98
    std::array<float, 4> unk_12e8{}; // +0x12E8
    FinalScoreTextLine unk_12f8; // +0x12F8
    ~FinalScoreSummaryPage();
};
struct FinalScoreServices
{
    GameSelectControlServices& frontend;
    SaveGameContext& saves; // 0x000F2D70's queued write.
    std::unique_ptr<FinalScoreState> state; // DAT_00506568
    std::unique_ptr<FinalScoreTally> tally; // DAT_00506558
    std::unique_ptr<i32> intro; // DAT_00506560, page 1: 1 the line queued, 2 the dialogue gone
    std::function<i32()> run_state; // DAT_00510F5C, the scene record's +0x104.
    std::unique_ptr<FinalScoreScoresPage> scores; // DAT_00506554
    std::unique_ptr<FinalScoreSuperScaryPage> super_scary; // DAT_0050656C
    std::unique_ptr<FinalScoreEndPage> end; // DAT_0050655C
    std::unique_ptr<FinalScoreSummaryPage> summary; // DAT_00506564
    u32 input_address = 0; // The scene input registry's address for the state's +0x170 device.
};
void final_score_control_object_sizes(ObjectSizes& output); // Xbox 0x00082D50
// The four rows' ranks and values (0x000D2FA0 clears them, 0x000D2F10 fills each), their sum and the save's +0x19C.
void build_runtime_final_score_tally(FinalScoreServices& services); // Xbox 0x000D2880
void clear_runtime_final_score_tally(FinalScoreTally& tally); // Xbox 0x000D2FA0
void fill_runtime_final_score_row(i32 row, FinalScoreServices& services); // Xbox 0x000D2F10, ECX row
// The rows' 0x00303F60 +0x10C callbacks: the save's stat for the current game (or the empty game's default), its
// rank and its text.
void rank_runtime_final_score_time(u16& rank, char16_t* value, FinalScoreServices& services); // Xbox 0x000D2950
void rank_runtime_final_score_kills(u16& rank, char16_t* value, FinalScoreServices& services); // Xbox 0x000D2B00
void rank_runtime_final_score_faints(u16& rank, char16_t* value, FinalScoreServices& services); // Xbox 0x000D2CB0
void rank_runtime_final_score_prisoners(u16& rank, char16_t* value,
    FinalScoreServices& services); // Xbox 0x000D2E10
// The state, the "finalscore" view, the score text, the keyboard device and the tally.
void create_runtime_final_score(FinalScoreServices& services); // Xbox 0x000D3E30
void destroy_runtime_final_score(FinalScoreServices& services); // Xbox 0x000D3FB0
void free_runtime_final_score_tally(FinalScoreServices& services); // Xbox 0x000D2830
void start_runtime_final_score_ditty(FinalScoreServices& services); // Xbox 0x000D44D0
void stop_runtime_final_score_ditty(FinalScoreServices& services); // Xbox 0x000D4500
void set_runtime_final_score_end_state(i32 next, FinalScoreServices& services); // Xbox 0x000D3370, ECX next
void start_runtime_final_score_end(FinalScoreServices& services); // Xbox 0x000D2FE0
void free_runtime_final_score_end(FinalScoreServices& services); // Xbox 0x000D3160
void tick_runtime_final_score_end(FinalScoreServices& services); // Xbox 0x000D3210
void draw_runtime_final_score_end(FinalScoreServices& services); // Xbox 0x000D3310
bool runtime_final_score_end_done(FinalScoreServices& services); // Xbox 0x000D3350
// Row `row`'s value, rank and icon on page 5.
void build_runtime_final_score_summary_entry(i32 row, FinalScoreServices& services); // Xbox 0x000D3C30, EAX row
// Page 5's panel: around the widest icon and the icons' stacked heights, by the HUD centre.
void layout_runtime_final_score_summary(FinalScoreServices& services); // Xbox 0x000D3990
void start_runtime_final_score_summary(FinalScoreServices& services); // Xbox 0x000D35A0
void free_runtime_final_score_summary(FinalScoreServices& services); // Xbox 0x000D3780
void tick_runtime_final_score_summary(FinalScoreServices& services); // Xbox 0x000D3860
void draw_runtime_final_score_summary(FinalScoreServices& services); // Xbox 0x000D38E0
bool runtime_final_score_summary_done(FinalScoreServices& services); // Xbox 0x000D3970
// The page's dialogue cutscene: kind 1 dialogpage1, 2 reallysad, 3 reallyaverage, 4 reallyhappy, else dialogpage1;
// on 0 the old one goes.
void set_runtime_final_score_cutscene(i32 on, i32 kind, FinalScoreServices& services); // Xbox 0x000D4280
// Page 1: the name, the tally, and the Baron's intro line (finalscore_intro1..6).
void open_runtime_final_score_intro(const char16_t* name, FinalScoreServices& services); // Xbox 0x000D4190, EAX
void start_runtime_final_score_intro(FinalScoreServices& services); // Xbox 0x000D3450
void free_runtime_final_score_intro(FinalScoreServices& services); // Xbox 0x000D3480
void tick_runtime_final_score_intro(FinalScoreServices& services); // Xbox 0x000D34D0
bool runtime_final_score_intro_done(FinalScoreServices& services); // Xbox 0x000D34F0
// Page 2: the name, the tally rebuilt, the old page freed and page 2 started (a tail jump to 0x000D1540).
void open_runtime_final_score_scores(const char16_t* name, FinalScoreServices& services); // Xbox 0x000D41E0, EAX
void start_runtime_final_score_scores(FinalScoreServices& services); // Xbox 0x000D1540
void free_runtime_final_score_scores(FinalScoreServices& services); // Xbox 0x000D18B0
void tick_runtime_final_score_scores(FinalScoreServices& services); // Xbox 0x000D1A80
void draw_runtime_final_score_scores(FinalScoreServices& services); // Xbox 0x000D1C70
bool runtime_final_score_scores_done(FinalScoreServices& services); // Xbox 0x000D1DA0
// The four rows' text and icons (0x000D24B0 each) and the icons spaced across the screen.
void lay_out_runtime_final_score_rows(FinalScoreServices& services); // Xbox 0x000D1DC0
void lay_out_runtime_final_score_row(i32 row, FinalScoreServices& services); // Xbox 0x000D24B0
void set_runtime_final_score_scores_state(i32 state, FinalScoreServices& services); // Xbox 0x000D1FC0, EAX
void fly_runtime_final_score_bat(FinalScoreServices& services); // Xbox 0x000D2730
i16 start_runtime_final_score_loop(const char* name, FinalScoreServices& services); // Xbox 0x000D27D0
void stop_runtime_final_score_loop(i16 voice, FinalScoreServices& services); // Xbox 0x000D2810, AX
// The Baron's line for a rank between `low` and `high`: XACT_SOUNDBANK_GBOY_0TO6 .. _21TO25 (0x00304F28).
const char* runtime_final_score_rank_line(u16 rank, u16 low, u16 high,
    FinalScoreServices& services); // Xbox 0x000D4570, SI rank, CX low, DX high
// Page 3: as 0x000D41E0, then 0x000D4610.
void open_runtime_final_score_super_scary(const char16_t* name,
    FinalScoreServices& services); // Xbox 0x000D4230, EAX
void start_runtime_final_score_super_scary(FinalScoreServices& services); // Xbox 0x000D4610
void free_runtime_final_score_super_scary(FinalScoreServices& services); // Xbox 0x000D4970
void tick_runtime_final_score_super_scary(FinalScoreServices& services); // Xbox 0x000D4A00
void draw_runtime_final_score_super_scary(FinalScoreServices& services); // Xbox 0x000D4B40
bool runtime_final_score_super_scary_done(FinalScoreServices& services); // Xbox 0x000D4BC0
void set_runtime_final_score_super_scary_state(i32 state, FinalScoreServices& services); // Xbox 0x000D4BE0, EAX
// The prompt's "totalscore_accept" row (0x003051AC): state 6, the prompt's request cleared and the prompt closed.
void accept_runtime_final_score_super_scary(FinalScoreServices& services); // Xbox 0x000D4E70
void tick_runtime_final_score(FinalScoreServices& services); // Xbox 0x000D4070
// Row 19 of the game-select mode table (8 bytes in retail, 0x000D79A0): a finished game's Info on Play shows the
// final score's Super Scary page (3), and the contents (row 3) come back when it is accepted.
struct GameFinalScoreWidget : GameSelectWidget
{
    i32 state = 0; // +0: 1 the page open, 2 done
    GameSelectControlState* owner = nullptr; // +4
};
GameSelectWidgetHandle create_runtime_game_final_score(GameSelectControlServices& services); // Xbox 0x000D79A0
void open_runtime_game_final_score(std::span<u8> owner, GameSelectControlState& control, GameSelectWidget& widget,
    GameSelectControlServices& services); // Xbox 0x000D79B0
void close_runtime_game_final_score(GameSelectWidget& widget,
    GameSelectControlServices& services); // Xbox 0x000D79F0
void tick_runtime_game_final_score(GameSelectWidget& widget,
    GameSelectControlServices& services); // Xbox 0x000D7A00
void draw_runtime_game_final_score(GameSelectWidget& widget,
    GameSelectControlServices& services); // Xbox 0x00082F70, the jump to 0x000D4120
void draw_runtime_final_score(FinalScoreServices& services); // Xbox 0x000D4120
// The control's vtable: construct 0x00082DC0, destroy 0x00082E20, update 0x00082E50 and draw 0x00082F70 (a jump
// to 0x000D4120).
bool construct_runtime_final_score_control(FinalScoreControlState& control, FinalScoreServices& services); // Xbox 0x00082DC0
void destroy_runtime_final_score_control(FinalScoreServices& services); // Xbox 0x00082E20
void tick_runtime_final_score_control(FinalScoreControlState& control, FinalScoreServices& services); // Xbox 0x00082E50
}
