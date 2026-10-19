#pragma once
#include "gameselect_rows.h"

namespace gbtg
{
struct Actor;
struct RuntimeDialogueTextPool;
struct RuntimeDialogueWidget;
struct RuntimeMovieState;

// Row 1 of the mode table: the slot-select widget (0xEAC bytes in retail, 0x000DD730). Members are in retail order
// with their retail offsets; the words that were pointers are host pointers.
struct SlotSelectWidget : GameSelectWidget
{
    // One of the three save plates (0x304 bytes from +0x214); the offsets are from the plate's start.
    struct Plate
    {
        i32 status = 0; // +0, 0x000DEB20: 1 empty, 2 saved, 3 damaged, 4 loading (no write)
        GameSelectTextLine label; // +4, the save name or the empty/damaged text; its record is ended by 0x0005F7E0
        GameSelectSprite plate_sprite; // +0x210
        GameSelectSprite disc_sprite; // +0x228, aid_texture_ghoulies_gameselect_disc_discspin
        GameSelectSprite nowrite_sprite; // +0x240
        GameSelectSprite broken_sprite; // +0x258
        std::array<char, 0x80> movie_name{}; // +0x270, from the movie rows at 0x0042941C / 0x00429290
        std::array<float, 2> movie_uv{}; // +0x2F0, the movie frame's lower UV (0x0005CA60)
        u32 box_color = 0; // +0x2F8, 0x00308B18 selected, 0x00308B1C at rest
        float box_width = 0.f; // +0x2FC
        float box_height = 0.f; // +0x300
    };
    i32 state = 0; // +0, 0x000DF9A0 sets it; 0x000DDD80 and 0x000DDF20 switch on it
    GameSelectTextLine prompt; // +4, the "Choose A Game" line; localtext 0x000FF120 writes its text
    RuntimeDialogueTextPool* prompt_pool = nullptr; // +0x210
    std::array<Plate, 3> plates{}; // +0x214
    std::array<float, 2> bat_target{}; // +0xB20, where 0x0006DA00 flies the bat
    // +0xB28 / +0xB2C are the cursor (row, slot): set together from 0x00428F9C / 0x00428FA0, compared and copied
    // together into +0xB30 / +0xB34. Only the slot moves on this screen (0x000E0210), so the row name is inferred.
    u32 cursor_row = 0; // +0xB28
    i32 slot = 0; // +0xB2C, the selected plate, 0..2
    u32 flown_row = 0; // +0xB30, the cursor the bat last flew to; 0xBADBADBA until it has
    u32 flown_slot = 0; // +0xB34
    Actor* bat = nullptr; // +0xB38
    RuntimeDialogueTextPool* plate_pool = nullptr; // +0xB3C
    GameSelectSprite background; // +0xB40
    GameSelectSprite old_background; // +0xB58, faded out
    float background_fade_timer = 0.f; // +0xB70, starts at the bat's flight time (Bat +0x93C)
    float background_fade = 0.f; // +0xB74, 0..1, 0x000DF330
    float plate_fade_timer = 0.f; // +0xB78, starts at DAT_00429014, 0x000DF3E0
    u32 plates_fading = 0; // +0xB7C, set by state 4, cleared when the fade ends
    RuntimeMovieState* movie = nullptr; // +0xB80
    i32 movie_frames = 0; // +0xB84, frames drawn since the movie was chosen
    u32 movie_held = 0; // +0xB88, the widget holds one of the movie's play_count
    float zoom_timer = 0.f; // +0xB8C, starts at DAT_00429008, 0x000DE7B0
    float fade_out_timer = 0.f; // +0xB90, state 2, starts at DAT_00429028, ticked by 0x0012B2A0
    i32 saved_plate_flag_0 = 0; // +0xB94, DAT_00510F7C while the widget is open
    i32 saved_plate_flag_1 = 0; // +0xB98, DAT_00510F80
    std::array<std::array<char16_t, 0x80>, 3> slot_names{}; // +0xB9C, each slot's save name
    i32 loads_pending = 0; // +0xE9C, save loads not yet called back (0x000E0640)
    i32 scan_steps = 0; // +0xEA0, save enumeration steps taken (0x000DF5D0 / 0x000DF650)
    GameSelectControlState* owner = nullptr; // +0xEA4
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
