#pragma once
#include "dialogue_measurement.h"
namespace gbtg
{
struct DialoguePageLayout
{
    const char16_t* text; // retail +0
    u32 retained; // +4
    float width; // +8
    i32 lines; // +0xC
    // +0x10: five line widths. [5] is the word after them (+0x24, DAT_004EE22C past the DAT_004EE208 page),
    // which 0x0005A630 reads for the line the draw loop wraps onto at its limit; nothing uses that position.
    std::array<float, 6> line_widths;
    const char16_t* next_text = nullptr; // Native pointer view of retail +4.
};
void measure_dialogue_page(DialoguePageLayout& page, i32 line_limit,
    float wrap_width, DialogueMeasurementContext& context); // Xbox 0x00059880
}
