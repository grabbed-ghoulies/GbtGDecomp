#pragma once
#include "gbtg/types.h"
#include <array>
namespace gbtg
{
struct AudioEventVoices;
struct DialogueSoundSwapNames
{
    std::array<const char16_t*, 18> boy_from; // PTR_004247A0
    std::array<const char16_t*, 9> girl_from; // PTR_004247E8
    std::array<const char16_t*, 18> girl_to; // PTR_00424810
    std::array<const char16_t*, 9> boy_to; // PTR_00424858
};
extern const DialogueSoundSwapNames g_retail_dialogue_sound_swaps;
struct DialogueSoundTagState
{
    std::array<char, 128> name; // DAT_004E6148
    std::array<char, 128> soundbank; // DAT_004E60C8
    std::array<char, 128> cue; // DAT_0042471C
};
void select_dialogue_sound_tag(const char16_t* text, u8 mode_flags, u32 scene_flags,
    DialogueSoundTagState& state, const DialogueSoundSwapNames& names); // Xbox 0x00057B00
void request_dialogue_page_voice(const DialogueSoundTagState& state,
    AudioEventVoices& voices); // Xbox 0x00057D10
}
