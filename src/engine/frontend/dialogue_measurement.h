#pragma once
#include "dialogue_tag_actions.h"
#include "dialogue_substitutions.h"
#include "dialogue_text_metrics.h"
namespace gbtg
{
struct DialogueMeasurementContext
{
    DialogueTagContext& tags;
    RuntimeTextMetrics& metrics;
    LocalTextResourceContext& localtext;
    DialogueBitsetView bits; // DAT_00506C54
    i32 first_count; // DAT_00506C7C
    i32 first_additional; // DAT_00545970
    i32 second_count; // DAT_00506C80
    i32 third_count; // DAT_00506C84
    i32 third_additional; // DAT_00545974
    i32 elapsed; // DAT_00506C78
    float elapsed_additional; // DAT_004312D0
    float initial_scale; // DAT_00424910
    float drawing_scale; // DAT_0042493C
    i32 paragraph_disabled; // DAT_0050444C
    char16_t* wide_scratch; // DAT_00507E78
    char* narrow_scratch; // DAT_00508EB8
};
float measure_dialogue_segment(const char16_t* text, bool paragraph,
    float scale, DialogueMeasurementContext& context); // Xbox 0x00059FE0
}
