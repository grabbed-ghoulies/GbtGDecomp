#pragma once
#include "player_health_display.h"
#include "dialogue_text_pool.h"
#include "../../gbtg/text/localtext_resources.h"
#include <vector>

namespace gbtg
{
// One scoreboard message row (retail stride 0x94). The +0x3C..+0x6F block is a HUD counter.
struct RuntimeScoreboardMessage
{
    u32 id; // +0.
    u32 kind; // +4.
    u32 word_8; // +8.
    float float_c; // +0xC.
    float float_10; // +0x10.
    u32 word_14; // +0x14.
    u32 word_18; // +0x18.
    u32 word_1c; // +0x1C.
    float scale_x; // +0x20.
    float scale_y; // +0x24.
    float float_28; // +0x28.
    float float_2c; // +0x2C.
    float float_30; // +0x30.
    float float_34; // +0x34.
    float float_38; // +0x38.
    RuntimeHudCounter counter; // +0x3C.
    u32 word_70; // +0x70.
    u32 word_74; // +0x74.
    u32 word_78; // +0x78.
    u32 pulse; // +0x7C.
    float pulse_scale; // +0x80.
    u32 icon; // +0x84.
    std::span<const u8> texture; // +0x88.
    u32 width; // +0x8C, texture width word.
    float scaled_width; // +0x90.
};
// The fields of a scoreboard (DAT_00545ABC, *DAT_005459F8) that the message calls read and write.
// The message pool header at +0 is the vector; its growth stands in for 0x000662A0.
struct RuntimeScoreboard
{
    std::vector<RuntimeScoreboardMessage> messages; // +0, stride 0x94.
    u32 next_id; // +4.
    std::span<const u8> first_icon; // +8.
    std::span<const u8> second_icon; // +0xC.
    // +0x10..+0x3C: three placements of {x0, y0, x1, y1}; 0x00130F20 picks one by message kind.
    std::array<float, 12> slots;
    float slot_scale_x; // +0x40.
    float slot_scale_y; // +0x44.
    float icon_scale; // +0x48.
    float spacing; // +0x4C.
    u32 word_50; // +0x50.
};
struct RuntimeScoreboardResources
{
    AnimationResourceRegistry& registry;
    ResourceErrorState& error;
    std::span<const u8> counter_preset; // DAT_00314170.
};
void acquire_runtime_scoreboard_icon(u32 index, std::span<const u8>& output,
    RuntimeScoreboardResources& context); // Xbox 0x00131BD0, EAX index
u32 add_runtime_scoreboard_message(u32 kind, u32 icon, u32 word_70, i32 count, u32 word_78, u32 pulse, u32 word_14,
    RuntimeScoreboard& board, RuntimeScoreboardResources& context); // Xbox 0x001316F0, EBX board
bool remove_runtime_scoreboard_message(u32 id, RuntimeScoreboard& board,
    AnimationResourceRegistry& registry); // Xbox 0x001319C0, EAX board, ECX id
void set_runtime_scoreboard_message_warning(u32 id, u32 warning,
    RuntimeScoreboard& board); // Xbox 0x00132620
u32 add_runtime_powerup_message(u32 kind, RuntimeScoreboard& board,
    RuntimeScoreboardResources& context); // Xbox 0x00132450, ECX kind
// The caption 0x00132540 prints when a powerup is awarded.
struct RuntimePowerupCaption
{
    LocalTextResourceContext& text;
    char16_t* scratch; // DAT_00507E78.
    std::span<const char* const> names; // DAT_0042ABF8, "powerupId_" names.
    RuntimeDialogueTextPool*& pool; // DAT_00545AB8.
    GhoulySpawnPool*& pools; // DAT_005043CC.
    const u16& hud_width; // DAT_005096E8.
    const float& hud_bottom; // DAT_0050974C.
    const RuntimeTextMetrics& metrics;
    float viewport_width;
    i32& next_id; // DAT_005043D0.
    u32& random_state; // DAT_0054DD7C.
};
void show_runtime_powerup_caption(u32 kind, RuntimePowerupCaption& context); // Xbox 0x00132540, EAX kind
// The two bonus counters 0x0012D7E0 shows on the second scoreboard.
struct RuntimeBonusCounters
{
    RuntimeScoreboard*& board; // DAT_005459F8.
    std::span<u32, 2> ids; // DAT_005459FC.
    std::span<u32, 2> active; // DAT_00545A04.
    std::span<i32, 2> counts; // DAT_00545A0C.
};
void show_runtime_bonus_counter(u32 slot, i32 count, RuntimeBonusCounters& counters,
    RuntimeScoreboardResources& context); // Xbox 0x0012D7E0, EDI slot
}
