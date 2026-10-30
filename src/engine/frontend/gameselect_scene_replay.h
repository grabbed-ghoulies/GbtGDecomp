#pragma once
#include "gameselect_rows.h"
#include "gbtg/entity.h"

namespace gbtg
{
struct RuntimeCutsceneInstance;
struct RuntimeDialogueTextPool;
struct RuntimeDialogueWidget;
struct GameSceneReplayWidget;
// What tells the replay pages apart: rows 9 (chapters), 10 (scenes), 11 (scenes with their bonus books) and 16
// (story books) run the same code over their own layout block in .data, camera, banner, prompts, menu items and
// open-widget global.
struct ReplayPage
{
    u32 row;
    u32 layout; // The page's floats: 0x004299B8, 0x00429A10, 0x00429A78 or 0x00429AE0.
    const char* camera; // 0x000496C0 / 0x00048CF0
    const char* banner; // The gameselect banner key
    u32 prompts; // The "ReplayChapterMenu" rows
    bool chapter_number; // The open reads the save's chapter into +0x490 (all but row 16)
    GameSceneReplayWidget* RuntimeFrontendState::*widget; // DAT_005065D0..DAT_005065DC
    void (*add_items)(GameSelectControlServices& services); // The state change's item builder
};
// Rows 9, 10, 11 and 16 of the mode table: a game's replay pages (0x49C bytes in retail, 0x000E1E00). The book
// page, the game's name, the banner, the prompts and a button-bat menu. Members are in retail order with their
// retail offsets; the words that were pointers are host pointers.
struct GameSceneReplayWidget : GameSelectWidget
{
    // +0, 0x000E3760 sets it: 0 opening, 1 waiting for the book, 2 the menu open, 3 leaving for another row, 4
    // back to the features.
    i32 state = 0;
    GameSelectTextLine title; // +4, the game's name
    GameSelectTextLine banner; // +0x210, replayscene_replayscenebanner
    RuntimeDialogueTextPool* pool = nullptr; // +0x41C
    RuntimeCutsceneInstance* page = nullptr; // +0x420, aid_cutscene_ghoulies_dialogs_dialogpage1
    EntityCamNode camera{}; // +0x424, "replayscene"
    i32 saved_plate_flag_0 = 0; // +0x484, DAT_00510F7C while the widget is open
    i32 saved_plate_flag_1 = 0; // +0x488, DAT_00510F80
    const char16_t* name = nullptr; // +0x48C, DAT_00429CC4
    i32 chapter_number = 0; // +0x490, the save's chapter's DAT_002FE698 entry; no translated code reads it
    GameSelectControlState* owner = nullptr; // +0x494
    RuntimeDialogueWidget* prompts = nullptr; // +0x498, "ReplayChapterMenu"
    const ReplayPage* kind = nullptr; // Host: which row's page this is.
};
// Row 9 (replay a chapter), row 11 (replay a scene, its bonus book marked) and row 16 (story books).
void open_runtime_game_chapter_replay(std::span<u8> owner, GameSelectControlState& control,
    GameSelectWidget& widget, GameSelectControlServices& services); // Xbox 0x000E1E10
void close_runtime_game_chapter_replay(GameSelectWidget& widget,
    GameSelectControlServices& services); // Xbox 0x000E21A0
void tick_runtime_game_chapter_replay(GameSelectWidget& widget,
    GameSelectControlServices& services); // Xbox 0x000E2230
void open_runtime_game_bonus_book_replay(std::span<u8> owner, GameSelectControlState& control,
    GameSelectWidget& widget, GameSelectControlServices& services); // Xbox 0x000E39F0
void close_runtime_game_bonus_book_replay(GameSelectWidget& widget,
    GameSelectControlServices& services); // Xbox 0x000E3D80
void tick_runtime_game_bonus_book_replay(GameSelectWidget& widget,
    GameSelectControlServices& services); // Xbox 0x000E3E10
void open_runtime_game_story_books(std::span<u8> owner, GameSelectControlState& control,
    GameSelectWidget& widget, GameSelectControlServices& services); // Xbox 0x000E48A0
void close_runtime_game_story_books(GameSelectWidget& widget,
    GameSelectControlServices& services); // Xbox 0x000E4BF0
void tick_runtime_game_story_books(GameSelectWidget& widget,
    GameSelectControlServices& services); // Xbox 0x000E4C80
void leave_runtime_game_story_books(GameSelectControlServices& services); // Xbox 0x000E4E70
void leave_runtime_game_chapter_replay(GameSelectControlServices& services); // Xbox 0x000E2360
void leave_runtime_game_bonus_book_replay(GameSelectControlServices& services); // Xbox 0x000E3F40
GameSelectWidgetHandle create_runtime_game_scene_replay(GameSelectControlServices& services); // Xbox 0x000E1E00
void open_runtime_game_scene_replay(std::span<u8> owner, GameSelectControlState& control, GameSelectWidget& widget,
    GameSelectControlServices& services); // Xbox 0x000E2B90
void close_runtime_game_scene_replay(GameSelectWidget& widget,
    GameSelectControlServices& services); // Xbox 0x000E2F20
void tick_runtime_game_scene_replay(GameSelectWidget& widget,
    GameSelectControlServices& services); // Xbox 0x000E2FB0
void draw_runtime_game_scene_replay(GameSelectWidget& widget,
    GameSelectControlServices& services); // Xbox 0x000E4DB0
void set_runtime_game_scene_replay_state(GameSceneReplayWidget& widget, i32 state,
    GameSelectControlServices& services); // Xbox 0x000E3760
// The scene-replay prompts' B callback (rows +0x44 at 0x003095D0). A and the stick are the features menu's
// 0x000E4E40 and 0x000D8080; the page keys (0x000E4ED0, 0x000E4EC0) jump to the button-bat menu's 0x00143F30 and
// 0x00143D60.
void leave_runtime_game_scene_replay(GameSelectControlServices& services); // Xbox 0x000E30E0
}
