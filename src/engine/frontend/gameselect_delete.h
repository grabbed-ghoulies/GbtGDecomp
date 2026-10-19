#pragma once
#include "gameselect_rows.h"
#include "gbtg/entity.h"

namespace gbtg
{
struct RuntimeCutsceneInstance;
struct RuntimeDialogueTextPool;
struct RuntimeDialogueWidget;
// Row 2 of the mode table: deleting a game (0x594 bytes in retail, 0x000DB720). Members are in retail order with
// their retail offsets; the words that were pointers are host pointers.
struct GameDeletionWidget : GameSelectWidget
{
    i32 state = 0; // +0, 0x000DBDC0 sets it; 0x000DB9E0 switches on it
    GameSelectTextLine title; // +4, the game's name, from +0x48C
    GameSelectTextLine prompt; // +0x210, deleteprompt, then deleting
    RuntimeDialogueTextPool* pool = nullptr; // +0x41C
    RuntimeCutsceneInstance* page = nullptr; // +0x420, aid_cutscene_ghoulies_dialogs_dialogpage1
    EntityCamNode camera{}; // +0x424, "gameslotdeletion"
    i32 saved_plate_flag_0 = 0; // +0x484, DAT_00510F7C while the widget is open
    i32 saved_plate_flag_1 = 0; // +0x488, DAT_00510F80
    std::array<char16_t, 0x80> save_name{}; // +0x48C, lstrcpyW from DAT_00429CC4
    GameSelectControlState* owner = nullptr; // +0x58C
    RuntimeDialogueWidget* prompts = nullptr; // +0x590, "GameSlotDeletionMenu"
};
GameSelectWidgetHandle create_runtime_game_deletion(GameSelectControlServices& services); // Xbox 0x000DB720
void open_runtime_game_deletion(std::span<u8> owner, GameSelectControlState& control, GameSelectWidget& widget,
    GameSelectControlServices& services); // Xbox 0x000DB730
void close_runtime_game_deletion(GameSelectWidget& widget, GameSelectControlServices& services); // Xbox 0x000DB950
void tick_runtime_game_deletion(GameSelectWidget& widget, GameSelectControlServices& services); // Xbox 0x000DB9E0
void draw_runtime_game_deletion(GameSelectWidget& widget, GameSelectControlServices& services); // Xbox 0x000DBB90
void set_runtime_game_deletion_state(GameDeletionWidget& widget, i32 state,
    GameSelectControlServices& services); // Xbox 0x000DBDC0, EAX the state
// The "GameSlotDeletionMenu" prompts' callbacks (rows +0x84 at 0x003077B0).
void leave_runtime_game_deletion(GameSelectControlServices& services); // Xbox 0x000DBD50
void confirm_runtime_game_deletion(GameSelectControlServices& services); // Xbox 0x000DBC20
}
