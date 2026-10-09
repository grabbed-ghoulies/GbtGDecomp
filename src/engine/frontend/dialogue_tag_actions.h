#pragma once
#include "dialogue_trophies.h"
#include "dialogue_sound_tags.h"
namespace gbtg
{
struct DialogueTagState
{
    i32 alignment; // DAT_0042495C
    i32 no_paragraph; // DAT_00424968
    i32 tab; // DAT_00424964
    i32 heads; // DAT_004EE294
    i32 buttons; // DAT_004EE290
    float time; // DAT_004EE298
    float scale_x; // DAT_00424934
    float scale_y; // DAT_00424938
    i32 invoke_map; // DAT_004EE2B0
    std::array<char, 64> numeric_scratch; // DAT_00507E38
};
struct DialogueTagContext
{
    DialogueTagState& state;
    DialogueTrophyContext& trophies;
    DialogueSoundTagState& sound;
    const DialogueTokenTable& tokens;
    const DialogueSoundSwapNames& sound_names;
    std::span<const i32> final_scores;
    i32 selected_score;
    i32 high_score;
    u8 mode_flags;
    u32 scene_flags;
};
double dialogue_wide_to_double(const char16_t* text, std::array<char, 64>& scratch); // Xbox 0x000FB810
void execute_dialogue_token(i32 token, const char16_t* arguments,
    DialogueTagContext& context); // Xbox 0x00056F80
const char16_t* process_dialogue_tag(const char16_t* text,
    DialogueTagContext& context); // Xbox 0x00056EA0
}
