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
// The game-select control's scalar fields at their retail offsets (300 bytes, 0x000835F0; see
// docs/RETAIL_LAYOUTS.md). +0x20 is the scene-control base's scene_control_field::elapsed. The words that hold
// pointers are GameSelectControlState members, noted here by offset.
namespace gameselect_control_field
{
constexpr u32 kSize = 300;
// +0x24 camera (EntityCamNode, to +0x84), +0x84 texture, +0x88 row, +0x8C widget
constexpr RetailField<u32, 0x090> current_row{}; // the mode-table row of the open widget
// A widget's request for the next row: when it is set and the book allows it (0x00083E20), 0x000838D0 closes the
// widget, opens that row and clears this.
constexpr RetailField<u32, 0x094> requested_row{};
constexpr RetailField<std::array<char, 0x80>, 0x098> script{}; // 0x00083D10 requests it; no translated code writes it
// 0x000838D0 starts the game while start_ready is set: 1 the save's script (0x00083C70), 2 +0x98's (0x00083D10).
constexpr RetailField<u32, 0x118> pending_start{};
// The scene flags the start sets (0x00083C70): 1 sets bits 2 and 4, 2 clears 2 and sets 4, 3 clears both.
constexpr RetailField<u32, 0x11C> start_mode{};
// +0x120 book
// How the menu book turns with the row change: 1 no turn (the book is released), 2 forward and 3 back
// (0x00048260's clip 0 or 1). Pages 2 and 3 hold the widget and book until the turn is far enough (0x00083E20,
// 0x00083C40).
constexpr RetailField<u32, 0x124> book_turn{};
// Set by 0x00083B10(1), inlined in Play (0x000DB570), when stream 2 is idle; otherwise Play requests row 0x12.
constexpr RetailField<u32, 0x128> start_ready{};
static_assert(retail_fits(kSize, start_ready));
} // namespace gameselect_control_field

// Native state of a game-select control (factory type 0x9A) whose retail words are pointers.
struct GameSelectControlState
{
    EntityCamNode camera{}; // +0x24, "gameselect"
    std::span<u8> texture; // +0x84, the 640x480 texture 0x000533A0 creates.
    const GameSelectWidgetRow* row = nullptr; // +0x88, a row of the mode table 0x0030A000.
    GameSelectWidgetHandle widget; // +0x8C
    RuntimeTransitionBook* book = nullptr; // +0x120, the menu book 0x00048260 allocates.
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
}
