#pragma once
#include "gameselect_rows.h"

namespace gbtg
{
struct Actor;
struct RuntimeDialogueTextPool;
struct RuntimeDialogueWidget;
struct RuntimeMovieState;
// The button sequence 0x00142E50 watches for (0x70 bytes in retail): buttons to hold, then buttons to press and
// release in order.
struct ButtonSequence
{
    // +0: 1 empty (0x00142E50), 2 waiting for the held buttons, 3 taking the presses (0x00142F00), 4 entered.
    u32 state = 0;
    u32 step = 0; // +4, the press reached
    std::array<u32, 20> presses{}; // +8, button masks, in order (0x000DB1A0)
    u32 last_press = 0; // +0x58, the last index in presses, 0xFFFFFFFF for none
    std::array<u32, 2> held{}; // +0x5C, button masks held throughout
    u32 last_held = 0; // +0x64, the last index in held, 0xFFFFFFFF for none
    u32 step_pressed = 0; // +0x68, the step's button went down
    u32 step_released = 0; // +0x6C, and came back up
};

// Row 3 of the mode table: a game's contents (0xF34 bytes in retail, 0x000D8AB0). Members are in retail order
// with their retail offsets; the words that were pointers are host pointers.
struct GameContentsWidget : GameSelectWidget
{
    // One of the three chapter panels (0x2C4 bytes from +4): Play, Challenges and Features. The offsets are from
    // the panel's start.
    struct Chapter
    {
        GameSelectTextLine name; // +0, localized from name_key (0x000D9C20)
        u32 name_key = 0; // +0x20C, the localtext key's address in the executable
        GameSelectSprite sprite; // +0x210
        std::array<char, 0x80> movie_name{}; // +0x228, from the rows at 0x00428C84 + 0x80
        RuntimeMovieState* movie = nullptr; // +0x2A8
        i32 movie_frames = 0; // +0x2AC, frames drawn, to 100; under 7 the panel texture shows
        u32 movie_frame_ready = 0; // +0x2B0, set once 0x00107360 has copied a frame
        i32 movie_held = 0; // +0x2B4, the widget's share of the movie's play_count
        u32 box_color = 0; // +0x2B8, 0x003069E8 selected, 0x003069EC at rest
        float box_width = 0.f; // +0x2BC
        float box_height = 0.f; // +0x2C0
    };
    i32 state = 0; // +0, 0x000DA580 sets it; 0x000D9450 and 0x000D9640 switch on it
    std::array<Chapter, 3> chapters{}; // +4
    RuntimeDialogueTextPool* name_pool = nullptr; // +0x850, the chapter names
    std::array<float, 2> bat_target{}; // +0x854, where 0x0006DA00 flies the bat; 0x0013B9D0 takes it by address
    i32 chapter_index = 0; // +0x85C, the selected panel, 0..2, kept in DAT_005462C8
    u32 flown_chapter = 0; // +0x860, the panel the bat last flew to; 0xBADBADBA until it has
    Actor* bat = nullptr; // +0x864
    GameSelectTextLine title; // +0x868, the game's name
    RuntimeDialogueTextPool* title_pool = nullptr; // +0xA74
    GameSelectTextLine progress; // +0xA78, "Chapter n Scene m : place" (0x000DA040)
    RuntimeDialogueTextPool* progress_pool = nullptr; // +0xC84
    GameSelectSprite background; // +0xC88
    GameSelectSprite old_background; // +0xCA0
    float background_fade_timer = 0.f; // +0xCB8, starts at the bat's flight time (Bat +0x93C)
    float background_fade = 0.f; // +0xCBC, 0..1, 0x000DA300
    float panel_fade_timer = 0.f; // +0xCC0, starts at DAT_0032B110, 0x000DA3B0
    u32 panels_fading = 0; // +0xCC4, set on open, cleared when the fade ends
    float splat_timer = 0.f; // +0xCC8
    GameSelectSprite splat; // +0xCCC, aid_texture_ghoulies_gameselect_completedsplat
    i32 splat_phase = 0; // +0xCE4, 0x000DAF10: 0 gone, 1 growing, 2 held, 3 shrinking
    GameSelectTextLine completed; // +0xCE8, gameslotcontents_completed
    RuntimeDialogueTextPool* completed_pool = nullptr; // +0xEF4
    std::array<float, 2> splat_full_scale{}; // +0xEF8, the scale the splat grows to
    GameSelectSprite challenge; // +0xF00, aid_texture_ghoulies_scoreboard_challenge_unplayed
    i32 saved_plate_flag_0 = 0; // +0xF18, DAT_00510F7C while the widget is open
    i32 saved_plate_flag_1 = 0; // +0xF1C, DAT_00510F80
    const char16_t* name = nullptr; // +0xF20, DAT_00429CC4
    // +0xF24: 0x000DA040 from the save's words +0x184 / +0x188: 1, 2, or 3 when finished. 3 shows the completed
    // line and splat (state 3) and the finished-game prompts (0x00306D00).
    i32 start_mode = 0;
    GameSelectControlState* owner = nullptr; // +0xF28
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
