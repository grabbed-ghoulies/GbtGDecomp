#pragma once
#include "gbtg/anim.h"
#include "gbtg/entity.h"
#include "gbtg/object_factory.h"
#include "gbtg/resource_error.h"
#include "../frontend/gameselect_rows.h"

#include <functional>

namespace gbtg
{
struct RuntimeDisplaySetupContext;
struct RuntimeTransitionBook;
// A game-select control (factory type 0x9A, 300 bytes in retail, 0x000835F0). Members are in retail order with
// their retail offsets; the words that were pointers are host pointers. +0x20, the scene-control base's elapsed
// time (scene_control_field::elapsed), stays in the factory object's bytes.
struct GameSelectControlState
{
    EntityCamNode camera{}; // +0x24, "gameselect"
    std::span<u8> texture; // +0x84, the 640x480 texture 0x000533A0 creates.
    const GameSelectWidgetRow* row = nullptr; // +0x88, a row of the mode table 0x0030A000.
    GameSelectWidgetHandle widget; // +0x8C
    u32 current_row = 0; // +0x90, the mode-table row of the open widget
    // +0x94: a widget's request for the next row. When it is set and the book allows it (0x00083E20), 0x000838D0
    // closes the widget, opens that row and clears this.
    u32 requested_row = 0;
    std::array<char, 0x80> script{}; // +0x98, the script 0x00083B90 queues and 0x00083D10 requests
    // +0x118: 0x000838D0 starts the game while start_ready is set: 1 the save's script (0x00083C70, queued by
    // 0x00083B10), 2 +0x98's (0x00083D10, queued by 0x00083B90).
    u32 pending_start = 0;
    // +0x11C: the scene flags the start sets (0x00083C70): 1 sets bits 2 and 4, 2 clears 2 and sets 4, 3 clears
    // both.
    u32 start_mode = 0;
    RuntimeTransitionBook* book = nullptr; // +0x120, the menu book 0x00048260 allocates.
    // +0x124: how the menu book turns with the row change: 1 no turn (the book is released), 2 forward and 3 back
    // (0x00048260's clip 0 or 1). Pages 2 and 3 hold the widget and book until the turn is far enough (0x00083E20,
    // 0x00083C40).
    u32 book_turn = 0;
    // +0x128: set by 0x00083B10(1), inlined in Play (0x000DB570), when stream 2 is idle; otherwise Play requests
    // row 0x12.
    u32 start_ready = 0;
};
void gameselect_control_object_sizes(ObjectSizes& output); // Xbox 0x000835F0
bool construct_runtime_gameselect_control(std::span<u8> state, GameSelectControlState& control,
    GameSelectControlServices& services); // Xbox 0x00083660
void destroy_runtime_gameselect_control(std::span<u8> state, GameSelectControlState& control,
    GameSelectControlServices& services); // Xbox 0x00083850
void tick_runtime_gameselect_control(std::span<u8> state, GameSelectControlState& control,
    GameSelectControlServices& services); // Xbox 0x000838D0
void draw_runtime_gameselect_control(std::span<u8> state, GameSelectControlState& control,
    GameSelectControlServices& services); // Xbox 0x000839E0
bool runtime_gameselect_book_busy(const GameSelectControlState& control,
    const GameSelectControlServices& services); // Xbox 0x00083C40, ECX the control
// Calls the held scene order's release callback (DAT_00511B68) on DAT_00511B64.
void release_held_scene_order(GameSelectControlServices& services);
void hold_runtime_scene_order(const char* name, GameSelectControlServices& services); // Xbox 0x001234B0, EAX name
void queue_runtime_gameselect_script(GameSelectControlState& control, const char* script, u32 mode,
    GameSelectControlServices& services); // Xbox 0x00083B90, ECX the control, EAX the script
void select_runtime_gameselect_row(const GameSelectWidgetRow*& row,
    GameSelectControlServices& services); // Xbox 0x000E5AE0, ESI the control's +0x88
}
