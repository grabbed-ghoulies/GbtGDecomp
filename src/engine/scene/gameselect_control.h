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
// Native state of a game-select control (factory type 0x9A) whose retail words are pointers.
struct GameSelectControlState
{
    EntityCamNode camera{}; // +0x24, "gameselect"
    std::span<u8> texture; // +0x84, the 640x480 texture 0x000533A0 creates.
    const GameSelectWidgetRow* row = nullptr; // +0x88, a row of the mode table 0x0030A000.
    GameSelectWidgetHandle widget; // +0x8C
    void* book = nullptr; // +0x120, the menu book 0x00048260 allocates.
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
bool runtime_gameselect_book_busy(std::span<const u8> state, const GameSelectControlState& control,
    const GameSelectControlServices& services); // Xbox 0x00083C40, ECX the control
void select_runtime_gameselect_row(const GameSelectWidgetRow*& row,
    GameSelectControlServices& services); // Xbox 0x000E5AE0, ESI the control's +0x88
// Set while type 0x9A is installed for development (the --gameselect-native transition test). Without it the
// native start stops at type 154 and the host front end runs game select (docs/NATIVE_FRONTEND.md).
extern bool g_native_game_select;
}
