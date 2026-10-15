#pragma once
#include "gameselect_rows.h"

namespace gbtg
{
struct Actor;
struct RuntimeDialogueTextPool;
struct RuntimeDialogueWidget;
struct RuntimeMovieState;
// The button sequence 0x00142E50 watches for (0x70 bytes): its state, the step reached, the up-to-20 buttons
// 0x000DB1A0 lists at +8 (the last index at +0x58), the held buttons at +0x5C (the last at +0x64), and the two
// step flags.
struct ButtonSequence
{
    std::array<u8, 0x70> bytes{};
};
// Row 3 of the mode table: a game's contents (0xF34 bytes, 0x000D8AB0). The three chapter records are 0x2C4 bytes
// from +4. Its scalar words stay at their retail offsets in `bytes`; the words holding pointers are the members.
struct GameContentsWidget : GameSelectWidget
{
    struct Chapter
    {
        std::span<const u8> texture; // +0x214
        RuntimeMovieState* movie = nullptr; // +0x2AC
    };
    std::array<u8, 0xF34> bytes{};
    std::array<Chapter, 3> chapters{};
    RuntimeDialogueTextPool* name_pool = nullptr; // +0x850, the chapter names.
    Actor* bat = nullptr; // +0x864
    RuntimeDialogueTextPool* title_pool = nullptr; // +0xA74, the game's name.
    RuntimeDialogueTextPool* progress_pool = nullptr; // +0xC84
    std::span<const u8> background; // +0xC88
    std::span<const u8> old_background; // +0xCA0
    std::span<const u8> splat; // +0xCCC, aid_texture_ghoulies_gameselect_completedsplat
    RuntimeDialogueTextPool* completed_pool = nullptr; // +0xEF4
    std::span<const u8> challenge; // +0xF00
    const char16_t* name = nullptr; // +0xF20, DAT_00429CC4.
    GameSelectControlState* owner = nullptr; // +0xF28
    std::span<u8> owner_state;
    RuntimeDialogueWidget* prompts = nullptr; // +0xF2C
    ButtonSequence* sequence = nullptr; // +0xF30
};
GameSelectWidgetHandle create_runtime_game_contents(GameSelectControlServices& services); // Xbox 0x000D8AB0
void open_runtime_game_contents(std::span<u8> owner, GameSelectControlState& control, GameSelectWidget& widget,
    GameSelectControlServices& services); // Xbox 0x000D8B10
void close_runtime_game_contents(GameSelectWidget& widget, GameSelectControlServices& services); // Xbox 0x000D9300
void tick_runtime_game_contents(GameSelectWidget& widget, GameSelectControlServices& services); // Xbox 0x000D9450
void draw_runtime_game_contents(GameSelectWidget& widget, GameSelectControlServices& services); // Xbox 0x000D9640
// The contents prompts' callbacks (rows +0x84 at 0x003069F0, 0x00306D00, 0x00307010 and 0x00307320).
void move_runtime_contents_cursor(GameSelectControlServices& services); // Xbox 0x000DB450
void leave_runtime_game_contents(GameSelectControlServices& services); // Xbox 0x000DB510
void play_runtime_game(GameSelectControlServices& services); // Xbox 0x000DB570
void show_runtime_game_stats(GameSelectControlServices& services); // Xbox 0x000DB5E0
void open_runtime_game_info(GameSelectControlServices& services); // Xbox 0x000DB620
void open_runtime_game_challenges(GameSelectControlServices& services); // Xbox 0x000DB670
void open_runtime_game_features(GameSelectControlServices& services); // Xbox 0x000DB6D0
}
