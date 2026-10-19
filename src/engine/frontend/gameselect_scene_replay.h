#pragma once
#include "gameselect_rows.h"
#include "gbtg/entity.h"

namespace gbtg
{
struct RuntimeCutsceneInstance;
struct RuntimeDialogueTextPool;
struct RuntimeDialogueWidget;
// Row 10 of the mode table: a game's scene replay (0x49C bytes in retail, 0x000E1E00, which rows 9, 11 and 16
// also create). The book page, the game's name, the "Replay Scene" banner, the prompts and a button-bat menu of
// the scenes before the save's. Members are in retail order with their retail offsets; the words that were
// pointers are host pointers.
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
};
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
