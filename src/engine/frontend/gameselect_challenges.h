#pragma once
#include "gameselect_rows.h"
#include "player_health_display.h"

namespace gbtg
{
struct Actor;
struct RuntimeDialogueTextPool;
struct RuntimeDialogueWidget;
// The challenge the menu last showed and what the chosen one hands its scene. All are BSS words game select
// never resets, so the choice survives the challenge and the trip back.
struct RuntimeChallengeSelection
{
    i32 selected = 0; // DAT_00506578, 0..20
    i32 previous = 0; // DAT_0050657C, the one before the last move
    i32 moved_left = 0; // DAT_00506580, the last move was to the left
    std::array<i32, 4> thresholds{}; // DAT_004E17DC, the chosen challenge's trophy scores (its params +0x140)
    std::array<char, 0x10C> key{}; // DAT_004E17EC, the chosen challenge's scenenames key (0x0004A3B0)
    std::array<char, 0x80> order{}; // DAT_004E1748, challenge 21's scene order (0x00049B60)
};
extern RuntimeChallengeSelection g_challenge_selection;

// One challenge of the menu (0x46C bytes in retail, 21 in a 0x5CDC heap block). Members are in retail order with
// their retail offsets.
struct GameChallengeEntry
{
    i32 index = 0; // +0, 0..20
    RuntimeHudCounter number{}; // +4, the challenge's number, drawn by 0x0012F000
    i32 word_38 = 0; // +0x38, 2; nothing reads it
    GameSelectTextLine name; // +0x3C, its scenenames text (or "locked"), centred on the screen
    std::array<char, 0x80> texture_name{}; // +0x248, its picture, or challenges_locked
    std::array<char, 0x80> script{}; // +0x2C8, the script 0x00083B90 queues
    std::array<char, 0x100> key{}; // +0x348, its scenenames key, or "challengelocked"
    std::array<i32, 4> thresholds{}; // +0x448, its params +0x140
    i32 trophy = 0; // +0x458, the save's word (DAT_004E194C): 0 unplayed, 3 bronze .. 6 platinum
    std::span<const u8> texture; // +0x45C, the picture while the entry is one of the five shown
    u32 color = 0; // +0x460
    u32 box_color = 0; // +0x464
    i32 unlocked = 0; // +0x468
};

// Row 6 of the mode table: the challenges (0x1E0 bytes in retail, 0x000D5850). Five of the challenges on a
// carousel over the selected one's picture, its trophy and number, the bonus books found, the prompts and the
// cursor bat. Members are in retail order with their retail offsets; the words that were pointers are host
// pointers.
struct GameChallengesWidget : GameSelectWidget
{
    GameSelectControlState* owner = nullptr; // +0
    // +4, 0x000D70E0 sets it: 1 the intro, 2 the menu open, 3 moving to another challenge, 4 its intro page, 5
    // leaving with the chosen challenge, 6 gone.
    i32 state = 0;
    std::array<char16_t, 0x80> name{}; // +8, the game's name (DAT_00429CC4)
    i32 count = 0; // +0x108, 20, or 21 once the progress bit 0x25 is set
    std::unique_ptr<std::array<GameChallengeEntry, 21>> entries; // +0x10C, 0x000F72D0(0x5CDC)
    std::array<GameChallengeEntry*, 5> shown{}; // +0x110, the carousel, the selected one third
    std::array<float, 2> bat_target{}; // +0x124, where 0x0006DA00 flies the bat
    Actor* bat = nullptr; // +0x12C
    // +0x130: the trophy icons, unplayed, bronze, silver, gold and platinum.
    std::array<std::span<const u8>, 5> trophies{};
    float trophy_drop = 0.f; // +0x144, half the bronze trophy's height
    RuntimeDialogueTextPool* pool = nullptr; // +0x148
    i32 chosen = 0; // +0x14C, A was pressed on an unlocked challenge
    GameSelectSprite picture; // +0x150, the selected challenge's picture over the screen
    GameSelectSprite old_picture; // +0x168, the one it replaces, faded out
    float fade_timer = 0.f; // +0x180, from the bat's flight time
    float fade = 0.f; // +0x184, 0..1
    float leave_timer = 0.f; // +0x188, from DAT_0032B2AC
    float leave = 0.f; // +0x18C, 0..1, the selected challenge grows
    float intro_timer = 0.f; // +0x190
    i32 intro = 0; // +0x194, cleared when the bat is made
    i32 saved_plate_flag_0 = 0; // +0x198, DAT_00510F7C while the widget is open
    i32 saved_plate_flag_1 = 0; // +0x19C, DAT_00510F80
    RuntimeDialogueWidget* prompts = nullptr; // +0x1A0, "challengeMenu_basic"
    RuntimeHudCounter books{}; // +0x1A4, the bonus books found (DAT_00506C54's bits)
    std::span<const u8> books_icon; // +0x1D8
    float books_icon_half_width = 0.f; // +0x1DC
};
GameSelectWidgetHandle create_runtime_game_challenges(GameSelectControlServices& services); // Xbox 0x000D5850
void open_runtime_game_challenges_menu(std::span<u8> owner, GameSelectControlState& control,
    GameSelectWidget& widget, GameSelectControlServices& services); // Xbox 0x000D5880
void close_runtime_game_challenges(GameSelectWidget& widget,
    GameSelectControlServices& services); // Xbox 0x000D62A0
void tick_runtime_game_challenges(GameSelectWidget& widget,
    GameSelectControlServices& services); // Xbox 0x000D6410
void draw_runtime_game_challenges(GameSelectWidget& widget,
    GameSelectControlServices& services); // Xbox 0x000D67F0
void set_runtime_game_challenges_state(GameChallengesWidget& widget, i32 state,
    GameSelectControlServices& services); // Xbox 0x000D70E0, EAX the widget
void select_runtime_challenge(GameChallengesWidget& widget, i32 index, i32 move,
    GameSelectControlServices& services); // Xbox 0x000D7640, ECX the widget, EAX the index
// The "challengeMenu_basic" prompts' callbacks (rows +0x84 at 0x00305CD8).
void play_runtime_challenge(GameSelectControlServices& services); // Xbox 0x000D7770
void move_runtime_challenge_cursor(GameSelectControlServices& services); // Xbox 0x000D77C0
void leave_runtime_game_challenges(GameSelectControlServices& services); // Xbox 0x000D7890
void show_runtime_challenge_intro(GameSelectControlServices& services); // Xbox 0x000D78E0
void open_runtime_challenge_bonus_books(GameSelectControlServices& services); // Xbox 0x000D78F0
}
