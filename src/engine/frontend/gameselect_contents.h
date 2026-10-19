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
// The contents widget's scalar fields at their retail offsets (see docs/RETAIL_LAYOUTS.md). The words that hold
// pointers are GameContentsWidget members, noted here by offset.
namespace game_contents_field
{
constexpr u32 kSize = 0xF34; // 0x000D8AB0 allocates 0xF34 bytes

// One of the three chapter panels (0x2C4 bytes from +4): Play, Challenges and Features.
namespace chapter
{
constexpr RetailRecord<0x000, gameselect_text_line::kSize> name{}; // localized from name_key (0x000D9C20)
constexpr RetailField<u32, 0x20C> name_key{}; // the localtext key's address in the executable
constexpr RetailRecord<0x210, gameselect_sprite::kSize> sprite{}; // texture +0x210 is Chapter::texture
constexpr RetailField<std::array<char, 0x80>, 0x228> movie_name{}; // from the rows at 0x00428C84 + 0x80
// +0x2A8 movie
constexpr RetailField<i32, 0x2AC> movie_frames{}; // frames drawn, to 100; under 7 the panel texture shows
constexpr RetailField<u32, 0x2B0> movie_frame_ready{}; // set once 0x00107360 has copied a frame
constexpr RetailField<i32, 0x2B4> movie_held{}; // the widget's share of the movie's play_count
constexpr RetailField<u32, 0x2B8> box_color{}; // 0x003069E8 selected, 0x003069EC at rest
constexpr RetailField<float, 0x2BC> box_width{};
constexpr RetailField<float, 0x2C0> box_height{};
constexpr u32 kSize = 0x2C4;
static_assert(retail_fits(kSize, box_height));
} // namespace chapter

constexpr RetailField<i32, 0x000> state{}; // 0x000DA580 sets it; 0x000D9450 and 0x000D9640 switch on it
constexpr RetailRecord<0x004, chapter::kSize, 3> chapters{};
// +0x850 name_pool
constexpr RetailField<float, 0x854> bat_target_x{}; // where 0x0006DA00 flies the bat; 0x0013B9D0 takes it by address
constexpr RetailField<float, 0x858> bat_target_y{};
constexpr RetailField<std::array<float, 2>, 0x854> bat_target{};
constexpr RetailField<i32, 0x85C> chapter_index{}; // the selected panel, 0..2, kept in DAT_005462C8
constexpr RetailField<u32, 0x860> flown_chapter{}; // the panel the bat last flew to; 0xBADBADBA until it has
// +0x864 bat
constexpr RetailRecord<0x868, gameselect_text_line::kSize> title{}; // the game's name
// +0xA74 title_pool
constexpr RetailRecord<0xA78, gameselect_text_line::kSize> progress{}; // "Chapter n Scene m : place" (0x000DA040)
// +0xC84 progress_pool
constexpr RetailRecord<0xC88, gameselect_sprite::kSize> background{}; // texture +0xC88 is background
constexpr RetailRecord<0xCA0, gameselect_sprite::kSize> old_background{}; // texture +0xCA0 is old_background
constexpr RetailField<float, 0xCB8> background_fade_timer{}; // starts at the bat's flight time (Bat +0x93C)
constexpr RetailField<float, 0xCBC> background_fade{}; // 0..1, 0x000DA300
constexpr RetailField<float, 0xCC0> panel_fade_timer{}; // starts at DAT_0032B110, 0x000DA3B0
constexpr RetailField<u32, 0xCC4> panels_fading{}; // set on open, cleared when the fade ends
constexpr RetailField<float, 0xCC8> splat_timer{};
constexpr RetailRecord<0xCCC, gameselect_sprite::kSize> splat{}; // texture +0xCCC is splat
constexpr RetailField<i32, 0xCE4> splat_phase{}; // 0x000DAF10: 0 gone, 1 growing, 2 held, 3 shrinking
constexpr RetailRecord<0xCE8, gameselect_text_line::kSize> completed{}; // gameslotcontents_completed
// +0xEF4 completed_pool
constexpr RetailField<std::array<float, 2>, 0xEF8> splat_full_scale{}; // the scale the splat grows to
constexpr RetailField<float, 0xEF8> splat_full_scale_x{};
constexpr RetailField<float, 0xEFC> splat_full_scale_y{};
constexpr RetailRecord<0xF00, gameselect_sprite::kSize> challenge{}; // texture +0xF00 is challenge
constexpr RetailField<i32, 0xF18> saved_plate_flag_0{}; // DAT_00510F7C while the widget is open
constexpr RetailField<i32, 0xF1C> saved_plate_flag_1{}; // DAT_00510F80
// +0xF20 name
// 0x000DA040 from the save's words +0x184 / +0x188: 1, 2, or 3 when finished. 3 shows the completed line and
// splat (state 3) and the finished-game prompts (0x00306D00).
constexpr RetailField<i32, 0xF24> start_mode{};
// +0xF28 owner, +0xF2C prompts, +0xF30 sequence
static_assert(retail_fits(kSize, start_mode));
static_assert(retail_fits(0x850, chapters));
} // namespace game_contents_field

// Row 3 of the mode table: a game's contents (0xF34 bytes, 0x000D8AB0). Its scalar fields stay at their retail
// offsets in `bytes` (game_contents_field above); the words holding pointers are the members.
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
