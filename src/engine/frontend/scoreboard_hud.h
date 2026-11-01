#pragma once
#include "brew_meter.h"
#include "hud_layout.h"
#include "scoreboard_messages.h"
#include <functional>
#include <memory>
#include <vector>

namespace gbtg
{
struct RuntimeScoreboardDraw;
struct RuntimeTextMetrics;
struct RuntimeTextFont;
struct LocalTextResourceContext;
struct GhoulySpawnPool;
// What the score captions (0x0012DCD0, 0x0012DDB0, 0x0012DFD0) reach beyond the board: the text pools, metrics
// and fonts, the localized terms and the caption texts.
struct RuntimeScoreCaptions
{
    GhoulySpawnPool*& text_pools; // DAT_005043CC, 0x0005F390
    const RuntimeTextMetrics& metrics; // 0x00060710, 0x0005F5C0
    std::span<const RuntimeTextFont* const> fonts; // DAT_00504444
    const float& wrap_width; // DAT_0050973C
    float viewport_width;
    i32& next_text_id;
    u32& random; // DAT_0054DD7C
    LocalTextResourceContext& localtext; // 0x000FF0A0
    std::span<char16_t, 0xA00> texts; // DAT_00432724, ten captions of 0x100 wide characters
    char16_t* scratch; // DAT_00507E78
    const u32& score_total; // DAT_0054597C
    std::span<const u8> pool_setup; // DAT_004326EC..DAT_004326F8: origin x, y, seed, a random seed when set
};
// The scoreboard's scene state that 0x00132040 sets up, 0x00132100 tears down and 0x00132290 draws from, apart
// from the meters the strategy already drives (brew, health), the powerup board and the score captions.
struct RuntimeScoreboardGlobals
{
    // DAT_00432688..DAT_004326DC: the total-damage counter.
    i32 damage_caption = -1; // DAT_00432688 (word, -1 when none).
    u32 damage_68c = 0; // DAT_0043268C.
    float damage_690 = 0.f; // DAT_00432690.
    u32 damage_694 = 0; // DAT_00432694.
    u32 damage_698 = 0; // DAT_00432698.
    u32 damage_69c = 0; // DAT_0043269C.
    RuntimeHudCounter damage_counter{}; // DAT_004326A0.
    u32 damage_6e0 = 0; // DAT_004326E0; the chair's 0x000CF0D0 sets 2.
    float damage_6d8 = 0.f; // DAT_004326D8, half the icon texture's +8 width.
    float damage_6e4 = 0.f; // DAT_004326E4.
    std::span<const u8> damage_texture; // DAT_004326D4.
    std::span<const u8> damage_flash_texture; // DAT_004326DC.
    // DAT_00433B4C..DAT_00433BD0: the meter that 0x0012EA50 draws.
    RuntimeDialogueTextPool* caption_pool = nullptr; // DAT_00433B4C.
    float caption_line = 0.f; // DAT_00433B50, the caption font's line drop at 0.97.
    u32 caption_color = 0xC0FFFFFFu; // DAT_00433B7C.
    float meter_level = 1.f; // DAT_00433B80.
    u16 meter_voice = 0xFFFF; // DAT_00433B84.
    u32 meter_visible = 0; // DAT_00433B88.
    u32 meter_b8c = 0; // DAT_00433B8C.
    u32 meter_b90 = 0; // DAT_00433B90.
    u32 meter_b94 = 0; // DAT_00433B94.
    u32 meter_b98 = 0; // DAT_00433B98.
    std::array<std::span<const u8>, 3> meter_textures; // DAT_00433B9C..DAT_00433BA4.
    std::array<float, 3> meter_scales{}; // DAT_00433BA8..DAT_00433BB0.
    u32 meter_bb4 = 0; // DAT_00433BB4.
    float meter_bb8 = 0.f; // DAT_00433BB8.
    u32 meter_bbc = 0; // DAT_00433BBC.
    float meter_bc0 = 0.f; // DAT_00433BC0.
    u32 meter_bc8 = 0; // DAT_00433BC8 (written by code not yet translated).
    u32 meter_drawn = 0; // DAT_00433BCC.
    u32 meter_bd0 = 0; // DAT_00433BD0: 0x0007DF50 sets it for a held kind-4 weapon; 0x0012ED30 buzzes and clears it.
    float meter_bd4 = 0.f; // DAT_00433BD4, half the gauge texture's +8 width.
    float meter_bd8 = 0.f; // DAT_00433BD8.
    // DAT_00433BE4..DAT_00433C6C: the health display's words that RuntimePlayerHealthDisplay does not view.
    u16 health_voice = 0xFFFF; // DAT_00433BE4.
    u16 health_second_voice = 0xFFFF; // DAT_00433BF0.
    std::array<float, 7> health_bf4{}; // DAT_00433BF4..DAT_00433C0C.
    u32 health_shown = 0; // DAT_00433C10.
    std::array<u32, 3> health_c18{}; // DAT_00433C18..DAT_00433C20 (DAT_00433C14 is the intensity).
    u32 health_pending = 0; // DAT_00433C24.
    std::array<u32, 4> health_c28{}; // DAT_00433C28..DAT_00433C34.
    u32 health_drawn = 0; // DAT_00433C6C.
    // DAT_00545994..DAT_005459A4: the brew meter's words that RuntimeBrewMeter does not view.
    u32 brew_visible = 0; // DAT_00545994.
    u32 brew_99c = 0; // DAT_0054599C.
    u32 brew_9a0 = 0; // DAT_005459A0.
    u32 brew_9a4 = 0; // DAT_005459A4.
    // DAT_005459F8..DAT_00545A10: the bonus board and its two counters.
    RuntimeScoreboard* bonus_board = nullptr; // DAT_005459F8.
    std::array<u32, 2> bonus_ids{}; // DAT_005459FC.
    std::array<u32, 2> bonus_active{}; // DAT_00545A04.
    std::array<i32, 2> bonus_counts{}; // DAT_00545A0C.
    u32 word_a18 = 0; // DAT_00545A18, the baddie meter shown (0x0012E5E0).
    u32 caption_tinted = 0; // DAT_00545A1C.
    u32 word_a20 = 0; // DAT_00545A20, the baddie meter's actor.
    u32 word_a24 = 0; // DAT_00545A24, the actor it is set against.
    float baddie_a28 = 0.f; // DAT_00545A28, the curse's countdown in seconds (0x0012E5E0's value).
    float baddie_a2c = 0.f; // DAT_00545A2C, the countdown below which the death curse intro shows.
    // DAT_00545A30..DAT_00545A60: the countdown's whole seconds and their digits (0x0012F990); +0x2C (DAT_00545A5C)
    // takes DAT_00433B6C at the start.
    RuntimeHudCounter baddie_counter{};
    Vec3 baddie_position{}; // DAT_00545A64, over the target's joint 4, raised by its +0x38.
    Vec3 baddie_offset{}; // DAT_00545A70, the view's sideways step between the meter's sprites.
    std::span<const u8> texture_a7c; // DAT_00545A7C.
    std::array<const u8*, 2> dollars{}; // DAT_00545AA8.
    RuntimeScoreboard* score_board = nullptr; // DAT_00545AB0.
    RuntimeDialogueTextPool* award_pool = nullptr; // DAT_00545AB8.
    // The boards 0x00130BB0 allocates, owned until 0x00130CE0 frees them.
    std::vector<std::unique_ptr<RuntimeScoreboard>> boards;
};

struct RuntimeScoreboardHud
{
    RuntimeScoreboardGlobals& globals;
    RuntimeBrewMeter& brew;
    RuntimePlayerHealthDisplay& health;
    std::array<const u8*, 10>& digits; // DAT_00545A80.
    RuntimeScoreboard*& powerup_board; // DAT_00545ABC.
    u32& enabled; // DAT_005493E0.
    u32& flags; // DAT_0050EA9C.
    u32& score_refresh; // DAT_004326E8.
    std::span<u32, 10> score_points; // DAT_00433B24.
    i32& health_style; // DAT_00433BE8.
    const i32& difficulty; // DAT_005064D8.
    const i32& prompt_lock; // DAT_004E1C34.
    const u32& prompt_owner; // DAT_004EE1EC.
    const RuntimeHudLayout& layout;
    const u16& width; // DAT_005096E8.
    std::span<const u8> texture_names; // 0x00313390: ten digit names, then two dollar names, 0x80 bytes each.
    u32 white; // DAT_00314F48.
    AnimationResourceRegistry& registry;
    ResourceErrorState& error;
    std::function<void(RuntimeDialogueWidget*)> destroy_widget; // 0x00142070.
    std::function<void(RuntimeDialogueTextPool*)> release_text_pool; // 0x0005F460.
    std::function<void(u16 voice)> stop_voice; // 0x00039690 with stack 0.
    std::function<bool()> area_dialogue_active; // 0x0005B830.
    RuntimeScoreboardDraw* draw = nullptr; // The HUD sprite queue and the draw clocks.
    RuntimeScoreCaptions* captions = nullptr; // The damage challenge's captions.
    std::array<std::span<const u8>, 2> counter_presets{}; // DAT_00314170, DAT_00314184.
    std::span<const u8> damage_preset{}; // DAT_00312F8C, the total-damage counter's digits.
    std::span<const u8> counter_preset(bool alternate) const
    {
        return counter_presets[alternate ? 1 : 0];
    }
};

void load_runtime_scoreboard_digits(RuntimeScoreboardHud& context); // Xbox 0x0012EE60
void release_runtime_scoreboard_digits(RuntimeScoreboardHud& context); // Xbox 0x0012EF10
void reset_runtime_brew_meter(RuntimeScoreboardHud& context); // Xbox 0x0012D180
void release_runtime_brew_meter(RuntimeScoreboardHud& context); // Xbox 0x0012D200
// A board with its message pool (stride 0x94, two rows), placed from `start` to `end`; its second and third
// placements step by `offset`, or repeat the first when `offset` is null.
void create_runtime_scoreboard(RuntimeScoreboard*& output, const float* start, const float* end,
    const float* offset, RuntimeScoreboardHud& context); // Xbox 0x00130BB0, ESI output, EBX end, EDI offset
void free_runtime_scoreboard(RuntimeScoreboard* board, RuntimeScoreboardHud& context); // Xbox 0x00130CE0
void create_runtime_bonus_board(RuntimeScoreboardHud& context); // Xbox 0x0012D5F0
void reset_runtime_damage_counter(RuntimeScoreboardHud& context); // Xbox 0x0012D930
void release_runtime_damage_counter(RuntimeScoreboardHud& context); // Xbox 0x0012D9B0
void create_runtime_score_board(RuntimeScoreboardHud& context); // Xbox 0x0012FA50
void reset_runtime_health_display(RuntimeScoreboardHud& context); // Xbox 0x0012FF80
void release_runtime_health_display(RuntimeScoreboardHud& context); // Xbox 0x00130080
void release_runtime_meter_textures(RuntimeScoreboardHud& context); // Xbox 0x0012E840
void create_runtime_powerup_board(RuntimeScoreboardHud& context); // Xbox 0x00132350
void set_up_runtime_scoreboard(RuntimeScoreboardHud& context); // Xbox 0x00132040
void tear_down_runtime_scoreboard(RuntimeScoreboardHud& context); // Xbox 0x00132100
void draw_runtime_health_display(RuntimeScoreboardHud& context); // Xbox 0x00130420
void draw_runtime_scoreboard(RuntimeScoreboard* board, RuntimeScoreboardHud& context); // Xbox 0x00130F20
void draw_runtime_damage_counter(RuntimeScoreboardHud& context); // Xbox 0x0012DAF0
void update_runtime_score_captions(RuntimeScoreboardHud& context); // Xbox 0x0012DDB0
void create_runtime_score_captions(RuntimeScoreboardHud& context); // Xbox 0x0012DCD0
// The number appended to `output` in groups of three (wsprintfW "%u", "%u,%03u", "%u,%03u,%03u").
void append_runtime_grouped_number(char16_t* output, u32 value); // Xbox 0x000FBD30, EAX value, EDI output
void draw_runtime_meter(RuntimeScoreboardHud& context); // Xbox 0x0012EA50
void draw_runtime_brew_meter(RuntimeScoreboardHud& context); // Xbox 0x0012D300
void draw_runtime_scoreboard_hud(RuntimeScoreboardHud& context); // Xbox 0x00132290
}
