#pragma once
#include "gameselect_rows.h"
#include "gbtg/entity.h"

namespace gbtg
{
struct ButtonBatMenuItem;
struct RuntimeCutsceneInstance;
struct RuntimeDialogueTextPool;
struct RuntimeDialogueWidget;
// Row 17 of the mode table: the game options (0xAC8 bytes in retail, 0x000E0B60). The book page, the game's name,
// the "Options" banner, the prompts and a button-bat menu of three options, each with its setting's text beside
// it. The settings are bits of the unlock block (DAT_00506C70): 4 rumble off, 2 the camera inverted, 8 Butler's
// Brew on. Members are in retail order with their retail offsets; the words that were pointers are host pointers.
struct GameOptionsWidget : GameSelectWidget
{
    // One option (0x210 bytes from +0x498): its menu item and its setting's text, placed beside it.
    struct Option
    {
        ButtonBatMenuItem* item = nullptr; // +0
        std::array<char16_t, 0x100> text{}; // +4
        float x = 0.f; // +0x204
        float y = 0.f; // +0x208
        u32 record = 0; // +0x20C, the 0x0005F5C0 text record
    };
    // +0, 0x000E1A80 sets it: 0 opening, 1 waiting for the book, 2 the menu open, 3 leaving, 4 back to the features.
    i32 state = 0;
    GameSelectTextLine title; // +4, the game's name
    GameSelectTextLine banner; // +0x210, options_optionsbanner
    RuntimeDialogueTextPool* pool = nullptr; // +0x41C
    RuntimeCutsceneInstance* page = nullptr; // +0x420, aid_cutscene_ghoulies_dialogs_dialogpage1
    EntityCamNode camera{}; // +0x424, "options"
    i32 saved_plate_flag_0 = 0; // +0x484, DAT_00510F7C while the widget is open
    i32 saved_plate_flag_1 = 0; // +0x488, DAT_00510F80
    const char16_t* name = nullptr; // +0x48C, DAT_00429CC4
    GameSelectControlState* owner = nullptr; // +0x490
    RuntimeDialogueWidget* prompts = nullptr; // +0x494, "OptionsMenuStandard"
    // +0x498: rumble (bit 4), the camera's rotation (bit 2) and Butler's Brew (bit 8).
    std::array<Option, 3> options{};
};
GameSelectWidgetHandle create_runtime_game_options(GameSelectControlServices& services); // Xbox 0x000E0B60
void open_runtime_game_options(std::span<u8> owner, GameSelectControlState& control, GameSelectWidget& widget,
    GameSelectControlServices& services); // Xbox 0x000E0B70
void close_runtime_game_options(GameSelectWidget& widget, GameSelectControlServices& services); // Xbox 0x000E0F50
void tick_runtime_game_options(GameSelectWidget& widget, GameSelectControlServices& services); // Xbox 0x000E1000
void draw_runtime_game_options(GameSelectWidget& widget, GameSelectControlServices& services); // Xbox 0x000E1130
void set_runtime_game_options_state(GameOptionsWidget& widget, i32 state,
    GameSelectControlServices& services); // Xbox 0x000E1A80, EBX the widget
// The "OptionsMenuStandard" / "OptionsMenuButlersBrew" prompts' B and Y (rows at 0x00308C20, 0x00308F30); A and
// the stick are the features menu's 0x000E4E40 and 0x000D8080.
void leave_runtime_game_options(GameSelectControlServices& services); // Xbox 0x000E11D0
void explain_runtime_butlers_brew(GameSelectControlServices& services); // Xbox 0x000E1890
}
