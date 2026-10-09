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
    std::array<float, 5> line_widths; // +0x10
    const char16_t* next_text = nullptr; // Native pointer view of retail +4.
};
void measure_dialogue_page(DialoguePageLayout& page, i32 line_limit,
    float wrap_width, DialogueMeasurementContext& context); // Xbox 0x00059880
}
