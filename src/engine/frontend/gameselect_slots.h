#pragma once
#include "gameselect_rows.h"

namespace gbtg
{
struct Actor;
struct RuntimeDialogueTextPool;
struct RuntimeDialogueWidget;
struct RuntimeMovieState;
// Row 1 of the mode table: the slot-select widget (0xEAC bytes, 0x000DD730). Its scalar words stay at their retail
// offsets in `bytes`; the words that hold pointers are the members below. Each of the three plates is 0x304 bytes
// from +0x214.
struct SlotSelectWidget : GameSelectWidget
{
    struct Plate
    {
        std::span<const u8> plate; // +0x424
        std::span<const u8> disc; // +0x43C, aid_texture_ghoulies_gameselect_disc_discspin
        std::span<const u8> nowrite; // +0x454
        std::span<const u8> broken; // +0x46C
    };
    std::array<u8, 0xEAC> bytes{};
    RuntimeDialogueTextPool* prompt_pool = nullptr; // +0x210
    std::array<Plate, 3> plates{};
    Actor* bat = nullptr; // +0xB38
    RuntimeDialogueTextPool* plate_pool = nullptr; // +0xB3C
    std::span<const u8> background; // +0xB40
    std::span<const u8> old_background; // +0xB58
    RuntimeMovieState* movie = nullptr; // +0xB80
    GameSelectControlState* owner = nullptr; // +0xEA4
    std::span<u8> owner_state; // +0xEA4's bytes
    RuntimeDialogueWidget* prompts = nullptr; // +0xEA8
};
GameSelectWidgetHandle create_runtime_slot_select(GameSelectControlServices& services); // Xbox 0x000DD730
void open_runtime_slot_select(std::span<u8> owner, GameSelectControlState& control, GameSelectWidget& widget,
    GameSelectControlServices& services); // Xbox 0x000DD740
void close_runtime_slot_select(GameSelectWidget& widget, GameSelectControlServices& services); // Xbox 0x000DDC90
void tick_runtime_slot_select(GameSelectWidget& widget, GameSelectControlServices& services); // Xbox 0x000DDD80
void draw_runtime_slot_select(GameSelectWidget& widget, GameSelectControlServices& services); // Xbox 0x000DDF20
void set_runtime_slot_select_state(SlotSelectWidget& widget, i32 state,
    GameSelectControlServices& services); // Xbox 0x000DF9A0
void open_runtime_gameselect_movies(GameSelectControlServices& services); // Xbox 0x000E01B0
// The slot prompts' callbacks (prompt rows +0x84 at 0x00308068, 0x00308378 and 0x00308688).
void move_runtime_slot_cursor(GameSelectControlServices& services); // Xbox 0x000E0210
void select_runtime_slot(GameSelectControlServices& services); // Xbox 0x000E02D0
void delete_runtime_slot(GameSelectControlServices& services); // Xbox 0x000E04F0
void leave_runtime_slot_select(GameSelectControlServices& services); // Xbox 0x000E05D0
}
