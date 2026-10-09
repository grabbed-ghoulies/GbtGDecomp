#include "dialogue_page_setup.h"
#include <bit>

namespace gbtg
{
// Xbox 0x000583C0
void prepare_dialogue_page(DialoguePageLayout& page, const char16_t* text,
    i32 line_limit, float wrap_width, DialogueMeasurementContext& context)
{
    bool found = false;
    while (*text != 0 && !found)
    {
        found = false;
        switch (*text)
        {
        case 9:
        case 10:
        case 13:
        case 32:
            break;
        case '{':
            text = process_dialogue_tag(text, context.tags);
            break;
        case '~':
            ++text;
            [[fallthrough]];
        default:
            page.text = text;
            found = true;
            break;
        }
        ++text;
    }
    measure_dialogue_page(page, line_limit, wrap_width, context);
}

// Xbox 0x000589F0
void reset_dialogue_page(DialoguePageLayout& page, const char16_t* text,
    u8 entry_flag, float wrap_width, DialoguePageSettings& settings,
    DialogueMeasurementContext& context)
{
    auto& state = context.tags.state;
    state.buttons = 1;
    state.heads = 1;
    settings.characters = 100;
    state.alignment = 0;
    state.tab = 0;
    state.no_paragraph = 0;
    state.time = std::bit_cast<float>(0x3DCCCCCDu); // DAT_0032B254
    const char* source = "XACT_SOUNDBANK_GMISC_UNSPECIFIED";
    i32 index = 0;
    char unit;
    do
    {
        unit = source[index];
        context.tags.sound.cue[index] = unit;
        ++index;
    }
    while (unit != 0);
    state.scale_y = std::bit_cast<float>(0x3F19999Au); // DAT_0032B248
    state.scale_x = std::bit_cast<float>(0x3F19999Au);
    settings.line_limit = 4;
    if (entry_flag == 0)
    {
        settings.line_limit = 5;
    }
    prepare_dialogue_page(page, text, settings.line_limit, wrap_width, context);
    settings.initial_alignment = state.alignment;
    if (state.heads != 0)
    {
        auto& resources = context.tags.trophies.resources;
        resources.previous_speaker = -1;
        resources.next_speaker = find_next_dialogue_speaker(text, resources.speaker, context.tags.tokens);
    }
}
}
