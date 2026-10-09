#pragma once
#include "dialogue_page_layout.h"
namespace gbtg
{
struct DialoguePageSettings
{
    i32 characters; // DAT_00424944
    i32 line_limit; // DAT_00424928
    i32 initial_alignment; // DAT_00424960
};
void prepare_dialogue_page(DialoguePageLayout& page, const char16_t* text,
    i32 line_limit, float wrap_width, DialogueMeasurementContext& context); // Xbox 0x000583C0
void reset_dialogue_page(DialoguePageLayout& page, const char16_t* text,
    u8 entry_flag, float wrap_width, DialoguePageSettings& settings,
    DialogueMeasurementContext& context); // Xbox 0x000589F0
}
