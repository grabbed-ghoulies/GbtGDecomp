#pragma once

#include "gbtg/types.h"
#include <array>

namespace gbtg
{
using DialogueTokenTable = std::array<const char16_t*, 0x5C>; // DAT_004E5F20
using DialogueTokenNames = std::array<const char*, 0x5C>; // PTR_DAT_004245A8
extern const DialogueTokenNames g_retail_dialogue_token_names;
void initialize_dialogue_tokens(DialogueTokenTable& output,
    const DialogueTokenNames& names); // Xbox 0x00056DF0
extern const DialogueTokenTable g_retail_dialogue_tokens; // native UTF-16 view of PTR_DAT_004245A8
i32 compare_dialogue_wide_prefix(const char16_t* first, const char16_t* second,
    u32 count); // Xbox 0x000FBDD0
u32 xapi_wide_text_length(const char16_t* text); // Xbox 0x0014AA3D
i32 find_dialogue_token(const char16_t* text, const DialogueTokenTable& tokens); // Xbox 0x00056F10
i32 find_next_dialogue_speaker(const char16_t* text, i32 current_speaker,
    const DialogueTokenTable& tokens); // Xbox 0x00057260
void set_dialogue_button_enabled(i32 requested, i32 button_allowed,
    i32& enabled); // Xbox 0x00058B50
}
