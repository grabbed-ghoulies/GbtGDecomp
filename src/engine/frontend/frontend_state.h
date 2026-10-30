#pragma once
#include "gbtg/types.h"
#include <array>
#include <span>
#include <vector>

namespace gbtg
{
struct FrontendPauseDitty;
struct RuntimeMovieState;
struct SlotSelectWidget;
struct GameNamingWidget;
struct GameContentsWidget;
struct GameDeletionWidget;
struct GameFeaturesWidget;
struct GameSceneReplayWidget;
struct GameChallengesWidget;
struct ButtonBatMenu;
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
    GameFeaturesWidget* features_widget = nullptr; // DAT_00506588, the open features widget.
    GameSceneReplayWidget* scene_replay_widget = nullptr; // DAT_005065D4, the open scene-replay widget.
    GameSceneReplayWidget* chapter_replay_widget = nullptr; // DAT_005065D0, the open chapter-replay widget.
    GameSceneReplayWidget* bonus_book_replay_widget = nullptr; // DAT_005065D8, the open bonus-book scenes widget.
    GameSceneReplayWidget* story_book_widget = nullptr; // DAT_005065DC, the open story-books widget.
    GameChallengesWidget* challenges_widget = nullptr; // DAT_00506574, the open challenges widget.
    // The heap block a replay's scene order is built in (0x000E3130); DAT_00511B64 points at it until its release
    // (0x000358F0) frees it. Host storage: the held span is a view.
    std::vector<u8> scene_order_heap;
    ButtonBatMenu* bat_menu = nullptr; // DAT_005461A4, the button-bat menu (0x00143550 .. 0x00143680).
    i32 contents_chapter = 0; // DAT_005462C8, the chapter contents last showed.
    i32 word_005065bc = 0; // DAT_005065BC, cleared by 0x00083B90 while stream 2 is busy; no translated code reads it
    i32 word_005065c0 = 0; // DAT_005065C0, set by 0x00083B90 then; no translated code reads it
};
extern RuntimeFrontendState g_frontend;
void reset_runtime_frontend(RuntimeFrontendState& state, FrontendPauseDitty& ditty); // Xbox 0x000E5A80
}
