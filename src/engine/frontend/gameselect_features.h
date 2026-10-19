#pragma once
#include "gameselect_rows.h"
#include "gbtg/entity.h"

namespace gbtg
{
struct RuntimeCutsceneInstance;
struct RuntimeDialogueTextPool;
struct RuntimeDialogueWidget;
// Row 8 of the mode table: a game's features (0x498 bytes in retail, 0x000D4EA0). The book page, the game's
// name, the "Features" banner, the prompts and the button-bat menu of features. Members are in retail order with
// their retail offsets; the words that were pointers are host pointers.
struct GameFeaturesWidget : GameSelectWidget
{
    // +0, 0x000D8840 sets it: 0 opening, 1 waiting for the book, 2 the menu open, 3 leaving for another row, 4
    // back to the contents.
    i32 state = 0;
    GameSelectTextLine title; // +4, the game's name
    GameSelectTextLine banner; // +0x210, features_featuresbanner
    RuntimeDialogueTextPool* pool = nullptr; // +0x41C
    RuntimeCutsceneInstance* page = nullptr; // +0x420, aid_cutscene_ghoulies_dialogs_dialogpage1
    EntityCamNode camera{}; // +0x424, "features"
    i32 saved_plate_flag_0 = 0; // +0x484, DAT_00510F7C while the widget is open
    i32 saved_plate_flag_1 = 0; // +0x488, DAT_00510F80
    const char16_t* name = nullptr; // +0x48C, DAT_00429CC4
    GameSelectControlState* owner = nullptr; // +0x490
    RuntimeDialogueWidget* prompts = nullptr; // +0x494, "FeaturesMenu"
};
GameSelectWidgetHandle create_runtime_game_features(GameSelectControlServices& services); // Xbox 0x000D4EA0
void open_runtime_game_features(std::span<u8> owner, GameSelectControlState& control, GameSelectWidget& widget,
    GameSelectControlServices& services); // Xbox 0x000D7A60
void close_runtime_game_features(GameSelectWidget& widget, GameSelectControlServices& services); // Xbox 0x000D7DB0
void tick_runtime_game_features(GameSelectWidget& widget, GameSelectControlServices& services); // Xbox 0x000D7E40
void draw_runtime_game_features(GameSelectWidget& widget, GameSelectControlServices& services); // Xbox 0x000D7F80
void set_runtime_game_features_state(GameFeaturesWidget& widget, i32 state,
    GameSelectControlServices& services); // Xbox 0x000D8840
// The "FeaturesMenu" prompts' callbacks (rows +0x84 at 0x00306570).
void activate_runtime_feature(GameSelectControlServices& services); // Xbox 0x000E4E40
void move_runtime_features_cursor(GameSelectControlServices& services); // Xbox 0x000D8080
void leave_runtime_game_features(GameSelectControlServices& services); // Xbox 0x000D8030
}
