#pragma once
#include "gameselect_rows.h"

namespace gbtg
{
struct RuntimeDialogueTextPool;
// Row 18 of the mode table: game select waits for music stream 2 (0x250 bytes in retail, 0x000E0650). The loading
// picture with a blinking "loading" line, until the stream is idle or the wait (DAT_005065C4) runs out. Members
// are in retail order with their retail offsets; the words that were pointers are host pointers.
struct GameStreamWaitWidget : GameSelectWidget
{
    i32 state = 0; // +0: 1 waiting, 2 done
    GameSelectSprite picture; // +4, aid_texture_ghoulies_gameselect_loading over the screen's centre
    GameSelectTextLine line; // +0x1C, "loading"
    RuntimeDialogueTextPool* pool = nullptr; // +0x228
    float blink = 0.f; // +0x22C, the line's on or off time left
    i32 shown = 0; // +0x230, the line is up
    i32 saved_plate_flag_0 = 0; // +0x234, DAT_00510F7C while the widget is open
    i32 saved_plate_flag_1 = 0; // +0x238, DAT_00510F80
    GameSelectControlState* owner = nullptr; // +0x23C
    i32 saved_row = 0; // +0x240, DAT_005065BC: the row a request held back (0x00083A50)
    i32 saved_turn = 0; // +0x244, DAT_005065C0: its book turn
    float wait = 0.f; // +0x248, DAT_005065C4: 0 waits for the stream, else this long
    float remaining = 0.f; // +0x24C
};
GameSelectWidgetHandle create_runtime_game_stream_wait(GameSelectControlServices& services); // Xbox 0x000E0650
void open_runtime_game_stream_wait(std::span<u8> owner, GameSelectControlState& control, GameSelectWidget& widget,
    GameSelectControlServices& services); // Xbox 0x000E0660
void close_runtime_game_stream_wait(GameSelectWidget& widget,
    GameSelectControlServices& services); // Xbox 0x000E0830
void tick_runtime_game_stream_wait(GameSelectWidget& widget,
    GameSelectControlServices& services); // Xbox 0x000E08C0
void draw_runtime_game_stream_wait(GameSelectWidget& widget,
    GameSelectControlServices& services); // Xbox 0x000E0A30
}
