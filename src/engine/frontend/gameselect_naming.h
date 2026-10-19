#pragma once
#include "gameselect_rows.h"
#include "gbtg/entity.h"

namespace gbtg
{
struct RuntimeCutsceneInstance;
struct VirtualKeyboard;
// Row 4 of the mode table: naming a new game (0x178 bytes, 0x000DCEB0). The book page cutscene behind the
// virtual keyboard, the keyboard and the edited name.
struct GameNamingWidget : GameSelectWidget
{
    i32 state = 0; // +0
    RuntimeCutsceneInstance* page = nullptr; // +4, aid_cutscene_ghoulies_dialogs_dialogpage1
    EntityCamNode camera{}; // +8, "gameslotnaming"
    VirtualKeyboard* keyboard = nullptr; // +0x68
    std::array<i32, 2> plate_flags{}; // +0x6C / +0x70, DAT_00510F7C / DAT_00510F80 while the widget is open.
    std::array<char16_t, 0x80> name{}; // +0x74
    GameSelectControlState* owner = nullptr; // +0x174
};
GameSelectWidgetHandle create_runtime_game_naming(GameSelectControlServices& services); // Xbox 0x000DCEB0
void open_runtime_game_naming(std::span<u8> owner, GameSelectControlState& control, GameSelectWidget& widget,
    GameSelectControlServices& services); // Xbox 0x000DCEC0
void close_runtime_game_naming(GameSelectWidget& widget, GameSelectControlServices& services); // Xbox 0x000DD030
void tick_runtime_game_naming(GameSelectWidget& widget, GameSelectControlServices& services); // Xbox 0x000DD0A0
void set_runtime_game_naming_state(GameNamingWidget& widget, i32 state,
    GameSelectControlServices& services); // Xbox 0x000DD6C0
void leave_runtime_game_naming(GameSelectControlServices& services); // Xbox 0x000DD550
void commit_runtime_game_name(const char16_t* name, GameSelectControlServices& services); // Xbox 0x000DD210
void finish_runtime_game_naming(GameSelectControlServices& services); // Xbox 0x000DD270
void draw_runtime_game_naming(GameSelectWidget& widget, GameSelectControlServices& services); // Xbox 0x000DD170
}
