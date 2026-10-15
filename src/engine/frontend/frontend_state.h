#pragma once
#include "gbtg/types.h"
#include <array>
#include <span>

namespace gbtg
{
struct FrontendPauseDitty;
struct RuntimeMovieState;
struct SlotSelectWidget;
struct GameNamingWidget;
struct GameContentsWidget;
struct GameDeletionWidget;
// The front end's boot-reset globals (0x000E5A80). DAT_00429BC0 (the next widget row) and DAT_00429BC4 (a wide
// name) are scene.cpp's g_429bc0 and g_429bc4.
struct RuntimeFrontendState
{
    std::span<u8> input; // DAT_00429BBC, a keyboard device (0x00107EF0).
    std::array<char16_t, 0x80> selected_name{}; // DAT_00429CC4
    std::array<std::array<char16_t, 0x80>, 3> slot_names{}; // DAT_00429DC4, stride 0x100, up to DAT_0042A0C4.
    i32 word_0042a0c4 = 0; // DAT_0042A0C4
    i32 word_0042a0c8 = 0; // DAT_0042A0C8
    float real_0042a0cc = 0.f; // DAT_0042A0CC
    // DAT_00506598..DAT_005065B4: the three empty-slot movies, then the five chapter movies (0x000E01B0).
    std::array<RuntimeMovieState*, 8> movies{};
    SlotSelectWidget* slot_widget = nullptr; // DAT_005065B8, the open slot-select widget.
    GameNamingWidget* naming_widget = nullptr; // DAT_00506594, the open naming widget.
    GameContentsWidget* contents_widget = nullptr; // DAT_005462CC, the open contents widget.
    GameDeletionWidget* deletion_widget = nullptr; // DAT_0050658C, the open deletion widget.
    i32 contents_chapter = 0; // DAT_005462C8, the chapter contents last showed.
};
extern RuntimeFrontendState g_frontend;
void reset_runtime_frontend(RuntimeFrontendState& state, FrontendPauseDitty& ditty); // Xbox 0x000E5A80
}
