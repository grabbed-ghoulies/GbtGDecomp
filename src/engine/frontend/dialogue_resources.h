#pragma once

#include "gbtg/anim.h"
#include "gbtg/resource_error.h"
#include "dialogue_tags.h"

namespace gbtg
{
struct DialogueOwnedResources
{
    i32 speaker = -1; // DAT_004E60A0
    std::span<const u8> speaker_resource; // DAT_004E60A4
    i32 previous_speaker = -1; // DAT_004E60A8
    i32 next_speaker = -1; // DAT_004E60AC
    u32 speaker_state = 0; // DAT_004E60B8
    u32 speaker_frame = 0; // DAT_004E60B0
    u32 speaker_height = 0; // DAT_004E60B4
    u32 icon = 0x36; // DAT_004E60C0
    std::span<const u8> icon_resource; // DAT_004E60C4
};
void format_dialogue_resource_name(char* output, const char* format, ...); // Xbox 0x00035830
void select_dialogue_icon(DialogueOwnedResources& state, i32 requested, u8 group,
    char* name_buffer, const DialogueTokenNames& names,
    AnimationResourceRegistry& registry, ResourceErrorState& error); // Xbox 0x00057970
void select_dialogue_speaker(DialogueOwnedResources& state, i32 requested,
    u8 mode_flags, u32 scene_flags, const DialogueTokenNames& names,
    AnimationResourceRegistry& registry, ResourceErrorState& error); // Xbox 0x000576D0
std::uintptr_t release_dialogue_speaker(DialogueOwnedResources& state,
    AnimationResourceRegistry& registry, std::uintptr_t mode_identity,
    u8 mode_flags, u32 scene_flags); // Xbox 0x00057400
void release_dialogue_icon(DialogueOwnedResources& state,
    AnimationResourceRegistry& registry); // Xbox 0x000577B0
}
