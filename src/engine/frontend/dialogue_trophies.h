#pragma once

#include "dialogue_resources.h"

namespace gbtg
{
i32 dialogue_score_tier(i32 score, const std::array<i32, 4>& thresholds); // Xbox 0x0004A400
void mark_dialogue_pause_icon(i32 token, u32& flags); // Xbox 0x000572D0
struct DialogueTrophyContext
{
    const std::array<i32, 4>& thresholds; // DAT_004E17DC
    DialogueOwnedResources& resources;
    u32& flags; // DAT_00424958
    char* name_buffer;
    const DialogueTokenNames& names;
    AnimationResourceRegistry& registry;
    ResourceErrorState& error;
};
void select_final_dialogue_trophy(std::span<const i32> scores, i32 selected,
    DialogueTrophyContext& context); // Xbox 0x00057330
void select_high_dialogue_trophy(i32 score, DialogueTrophyContext& context); // Xbox 0x000573A0
}
