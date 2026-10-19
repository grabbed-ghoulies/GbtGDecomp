#pragma once
#include "gameselect_rows.h"
#include "gbtg/retail_field.h"

namespace gbtg
{
struct Actor;
struct RuntimeDialogueTextPool;
struct RuntimeDialogueWidget;
struct RuntimeMovieState;

// The slot-select widget's scalar fields at their retail offsets (see docs/RETAIL_LAYOUTS.md). The words that hold
// pointers are SlotSelectWidget members, noted here by offset.
namespace slot_select_field
{
constexpr u32 kSize = 0xEAC; // 0x000DD730 allocates 0xEAC bytes

// One of the three save plates (0x304 bytes from +0x214).
namespace plate
{
constexpr RetailField<i32, 0x000> status{}; // 0x000DEB20: 1 empty, 2 saved, 3 damaged, 4 loading (no write)
// The save name or the empty/damaged text (gameselect_text_line); its record is ended by 0x0005F7E0.
constexpr RetailRecord<0x004, gameselect_text_line::kSize> label{};
constexpr RetailRecord<0x210, gameselect_sprite::kSize> plate_sprite{}; // texture +0x210 is Plate::plate
constexpr RetailRecord<0x228, gameselect_sprite::kSize> disc_sprite{}; // texture +0x228 is Plate::disc
constexpr RetailRecord<0x240, gameselect_sprite::kSize> nowrite_sprite{}; // texture +0x240 is Plate::nowrite
constexpr RetailRecord<0x258, gameselect_sprite::kSize> broken_sprite{}; // texture +0x258 is Plate::broken
constexpr RetailField<std::array<char, 0x80>, 0x270> movie_name{}; // from the movie rows at 0x0042941C / 0x00429290
constexpr RetailField<std::array<float, 2>, 0x2F0> movie_uv{}; // the movie frame's lower UV (0x0005CA60)
constexpr RetailField<u32, 0x2F8> box_color{}; // 0x00308B18 selected, 0x00308B1C at rest
constexpr RetailField<float, 0x2FC> box_width{};
constexpr RetailField<float, 0x300> box_height{};
constexpr u32 kSize = 0x304;
static_assert(retail_fits(kSize, box_height));
} // namespace plate

constexpr RetailField<i32, 0x000> state{}; // 0x000DF9A0 sets it; 0x000DDD80 and 0x000DDF20 switch on it
// The "Choose A Game" line (gameselect_text_line); localtext 0x000FF120 writes its text.
constexpr RetailRecord<0x004, gameselect_text_line::kSize> prompt{};
// +0x210 prompt_pool
constexpr RetailRecord<0x214, plate::kSize, 3> plates{};
constexpr RetailField<std::array<float, 2>, 0xB20> bat_target{}; // where 0x0006DA00 flies the bat
constexpr RetailField<float, 0xB20> bat_target_x{};
constexpr RetailField<float, 0xB24> bat_target_y{};
// +0xB28 / +0xB2C are the cursor (row, slot): set together from 0x00428F9C / 0x00428FA0, compared and copied
// together into +0xB30 / +0xB34. Only the slot moves on this screen (0x000E0210), so the row name is inferred.
constexpr RetailField<u32, 0xB28> cursor_row{};
constexpr RetailField<i32, 0xB2C> slot{}; // the selected plate, 0..2
constexpr RetailField<u32, 0xB30> flown_row{}; // the cursor the bat last flew to; 0xBADBADBA until it has
constexpr RetailField<u32, 0xB34> flown_slot{};
// +0xB38 bat, +0xB3C plate_pool
constexpr RetailRecord<0xB40, gameselect_sprite::kSize> background{}; // texture +0xB40 is background
constexpr RetailRecord<0xB58, gameselect_sprite::kSize> old_background{}; // texture +0xB58 is old_background, faded out
constexpr RetailField<float, 0xB70> background_fade_timer{}; // starts at the bat's flight time (Bat +0x93C)
constexpr RetailField<float, 0xB74> background_fade{}; // 0..1, 0x000DF330
constexpr RetailField<float, 0xB78> plate_fade_timer{}; // starts at DAT_00429014, 0x000DF3E0
constexpr RetailField<u32, 0xB7C> plates_fading{}; // set by state 4, cleared when the fade ends
// +0xB80 movie
constexpr RetailField<i32, 0xB84> movie_frames{}; // frames drawn since the movie was chosen
constexpr RetailField<u32, 0xB88> movie_held{}; // the widget holds one of the movie's play_count
constexpr RetailField<float, 0xB8C> zoom_timer{}; // starts at DAT_00429008, 0x000DE7B0
constexpr RetailField<float, 0xB90> fade_out_timer{}; // state 2, starts at DAT_00429028, ticked by 0x0012B2A0
constexpr RetailField<i32, 0xB94> saved_plate_flag_0{}; // DAT_00510F7C while the widget is open
constexpr RetailField<i32, 0xB98> saved_plate_flag_1{}; // DAT_00510F80
constexpr RetailRecord<0xB9C, 0x100, 3> slot_names{}; // each slot's save name, wide, 0x80 characters
constexpr RetailField<i32, 0xE9C> loads_pending{}; // save loads not yet called back (0x000E0640)
constexpr RetailField<i32, 0xEA0> scan_steps{}; // save enumeration steps taken (0x000DF5D0 / 0x000DF650)
// +0xEA4 owner, +0xEA8 prompts
static_assert(retail_fits(kSize, scan_steps));
static_assert(retail_fits(kSize, plates));
} // namespace slot_select_field

// Row 1 of the mode table: the slot-select widget (0xEAC bytes, 0x000DD730). Its scalar fields stay at their retail
// offsets in `bytes` (slot_select_field above); the words that hold pointers are the members below.
struct SlotSelectWidget : GameSelectWidget
{
    struct Plate
    {
        std::span<const u8> plate; // +0x424
        std::span<const u8> disc; // +0x43C, aid_texture_ghoulies_gameselect_disc_discspin
        std::span<const u8> nowrite; // +0x454
        std::span<const u8> broken; // +0x46C
    };
    std::array<u8, 0xEAC> bytes{};
    RuntimeDialogueTextPool* prompt_pool = nullptr; // +0x210
    std::array<Plate, 3> plates{};
    Actor* bat = nullptr; // +0xB38
    RuntimeDialogueTextPool* plate_pool = nullptr; // +0xB3C
    std::span<const u8> background; // +0xB40
    std::span<const u8> old_background; // +0xB58
    RuntimeMovieState* movie = nullptr; // +0xB80
    GameSelectControlState* owner = nullptr; // +0xEA4
    std::span<u8> owner_state; // +0xEA4's bytes
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
