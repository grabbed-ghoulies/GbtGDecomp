#include "dialogue_page_layout.h"
#include "../runtime/crt_format.h"
#include <bit>

namespace gbtg
{
// Xbox 0x00059880
void measure_dialogue_page(DialoguePageLayout& page, i32 line_limit,
    float wrap_width, DialogueMeasurementContext& context)
{
    const float scale = context.tags.state.scale_x;
    const char16_t* text = page.text;
    float current_width = 0.f;
    float maximum = 0.f;
    i32 line = 0;
    u16 previous = 0;
    context.tags.trophies.flags = 0;
    const float space = runtime_text_space_advance(*context.metrics.fonts[1], scale, context.metrics.scale_x);
    page.line_widths[0] = -1.f;
    page.line_widths[1] = -1.f;
    page.line_widths[2] = -1.f;
    page.line_widths[3] = -1.f;
    page.line_widths[4] = -1.f;
    context.metrics.spacing = 2.f; // DAT_005043F4 <- DAT_0032B110
    u32 flags = 2;
    bool paragraph;
    if (context.tags.state.no_paragraph == 0 && context.paragraph_disabled == 0)
    {
        paragraph = true;
    }
    else
    {
        paragraph = false;
    }
    page.line_widths[0] = measure_dialogue_segment(text, paragraph, context.tags.state.scale_x, context);
    while (*text != 0 && (flags & 1) == 0 && line < line_limit)
    {
        const u16 current = *text;
        if (current == '{')
        {
            text = process_dialogue_tag(text, context.tags);
            flags = context.tags.trophies.flags;
            context.tags.trophies.flags = 0;
            if (flags != 0)
            {
                if ((flags & 4) != 0)
                {
                    if (current_width > maximum)
                    {
                        maximum = current_width;
                    }
                    ++line;
                    if (line < line_limit)
                    {
                        if ((flags & 2) != 0 && context.tags.state.no_paragraph == 0 && context.paragraph_disabled == 0)
                        {
                            paragraph = true;
                        }
                        else
                        {
                            paragraph = false;
                        }
                        page.line_widths[line] = measure_dialogue_segment(text + 1, paragraph, scale, context);
                    }
                    current_width = 0.f;
                }
                if ((flags & 8) != 0)
                {
                    current_width = static_cast<float>(read_u16(context.tags.trophies.resources.icon_resource, 8)) * scale + current_width;
                }
                if ((flags & 0x10) != 0)
                {
                    float seed_scale = scale;
                    if (scale == 0.f)
                    {
                        seed_scale = context.drawing_scale;
                    }
                    current_width = seed_scale * 50.f + current_width;
                }
                char16_t* replacement;
                if ((flags & 0x40) != 0)
                {
                    dialogue_total_first(replacement, context.wide_scratch, context.narrow_scratch, context.first_count, context.first_additional);
                    current_width = measure_dialogue_segment(replacement, false, scale, context) + current_width;
                }
                if ((flags & 0x80) != 0)
                {
                    dialogue_second_count(replacement, context.wide_scratch, context.narrow_scratch, context.second_count);
                    current_width = measure_dialogue_segment(replacement, false, scale, context) + current_width;
                }
                if ((flags & 0x100) != 0)
                {
                    dialogue_total_third(replacement, context.wide_scratch, context.narrow_scratch, context.third_count, context.third_additional);
                    current_width = measure_dialogue_segment(replacement, false, scale, context) + current_width;
                }
                if ((flags & 0x20) != 0)
                {
                    dialogue_bit_count(replacement, context.wide_scratch, context.narrow_scratch, context.bits);
                    current_width = measure_dialogue_segment(replacement, false, scale, context) + current_width;
                }
                if ((flags & 0x200) != 0)
                {
                    context.wide_scratch[0] = 0;
                    const float elapsed = static_cast<float>(context.elapsed) + context.elapsed_additional;
                    append_dialogue_minutes(context.wide_scratch, elapsed, 1, context.localtext);
                    replacement = context.wide_scratch;
                    current_width = measure_dialogue_segment(replacement, false, scale, context) + current_width;
                }
                if ((flags & 0x800) != 0)
                {
                    context.wide_scratch[0] = 0;
                    scene_detail::crt_format_wide_string(context.wide_scratch, u"%d", context.tags.trophies.thresholds[0]);
                    replacement = context.wide_scratch;
                    current_width = measure_dialogue_segment(replacement, false, scale, context) + current_width;
                }
                if ((flags & 0x1000) != 0)
                {
                    context.wide_scratch[0] = 0;
                    scene_detail::crt_format_wide_string(context.wide_scratch, u"%d", context.tags.trophies.thresholds[1]);
                    replacement = context.wide_scratch;
                    current_width = measure_dialogue_segment(replacement, false, scale, context) + current_width;
                }
                if ((flags & 0x2000) != 0)
                {
                    context.wide_scratch[0] = 0;
                    scene_detail::crt_format_wide_string(context.wide_scratch, u"%d", context.tags.trophies.thresholds[2]);
                    replacement = context.wide_scratch;
                    current_width = measure_dialogue_segment(replacement, false, scale, context) + current_width;
                }
                if ((flags & 0x4000) != 0)
                {
                    context.wide_scratch[0] = 0;
                    scene_detail::crt_format_wide_string(context.wide_scratch, u"%d", context.tags.trophies.thresholds[3]);
                    replacement = context.wide_scratch;
                    current_width = measure_dialogue_segment(replacement, false, scale, context) + current_width;
                }
                if ((flags & 0x80000) != 0)
                {
                    context.wide_scratch[0] = 0;
                    append_dialogue_seconds(context.wide_scratch, static_cast<float>(context.tags.trophies.thresholds[0]), 1, context.localtext);
                    replacement = context.wide_scratch;
                    current_width = measure_dialogue_segment(replacement, false, scale, context) + current_width;
                }
                if ((flags & 0x100000) != 0)
                {
                    context.wide_scratch[0] = 0;
                    append_dialogue_seconds(context.wide_scratch, static_cast<float>(context.tags.trophies.thresholds[1]), 1, context.localtext);
                    replacement = context.wide_scratch;
                    current_width = measure_dialogue_segment(replacement, false, scale, context) + current_width;
                }
                if ((flags & 0x200000) != 0)
                {
                    context.wide_scratch[0] = 0;
                    append_dialogue_seconds(context.wide_scratch, static_cast<float>(context.tags.trophies.thresholds[2]), 1, context.localtext);
                    replacement = context.wide_scratch;
                    current_width = measure_dialogue_segment(replacement, false, scale, context) + current_width;
                }
                if ((flags & 0x400000) != 0)
                {
                    context.wide_scratch[0] = 0;
                    append_dialogue_seconds(context.wide_scratch, static_cast<float>(context.tags.trophies.thresholds[3]), 1, context.localtext);
                    replacement = context.wide_scratch;
                    current_width = measure_dialogue_segment(replacement, false, scale, context) + current_width;
                }
                if ((flags & 0x8000) != 0)
                {
                    context.wide_scratch[0] = 0;
                    scene_detail::crt_format_wide_string(context.wide_scratch, u"%d", context.tags.high_score);
                    replacement = context.wide_scratch;
                    current_width = measure_dialogue_segment(replacement, false, scale, context) + current_width;
                }
                if ((flags & 0x10000) != 0)
                {
                    context.wide_scratch[0] = 0;
                    append_dialogue_seconds(context.wide_scratch, static_cast<float>(context.tags.high_score), 1, context.localtext);
                    replacement = context.wide_scratch;
                    current_width = measure_dialogue_segment(replacement, false, scale, context) + current_width;
                }
                if ((flags & 0x20000) != 0)
                {
                    context.wide_scratch[0] = 0;
                    const i32 score = context.tags.final_scores[context.tags.selected_score];
                    if (score == -1)
                    {
                        copy_xapi_wide_text(context.wide_scratch, u"?");
                    }
                    else
                    {
                        scene_detail::crt_format_wide_string(context.wide_scratch, u"%d", score);
                    }
                    replacement = context.wide_scratch;
                    current_width = measure_dialogue_segment(replacement, false, scale, context) + current_width;
                }
                if ((flags & 0x40000) != 0)
                {
                    context.wide_scratch[0] = 0;
                    const i32 score = context.tags.final_scores[context.tags.selected_score];
                    if (score == -1)
                    {
                        copy_xapi_wide_text(context.wide_scratch, u"?");
                    }
                    else
                    {
                        append_dialogue_seconds(context.wide_scratch, static_cast<float>(score), 1, context.localtext);
                    }
                    replacement = context.wide_scratch;
                    current_width = measure_dialogue_segment(replacement, false, scale, context) + current_width;
                }
            }
        }
        else if (current == ' ')
        {
            const float word = runtime_text_word_advance(context.metrics, 1, text + 1, '{', scale);
            const float combined = (word + space) + current_width;
            if (combined > wrap_width)
            {
                if (current_width > maximum)
                {
                    maximum = current_width;
                }
                ++line;
                if (line < line_limit)
                {
                    page.line_widths[line] = current_width;
                }
                current_width = 0.f;
            }
            else
            {
                current_width = space + current_width;
            }
        }
        else
        {
            float character_scale;
            if ((flags & 2) != 0 && context.tags.state.no_paragraph == 0 && context.paragraph_disabled == 0)
            {
                character_scale = scale * 2.f;
            }
            else
            {
                character_scale = scale;
            }
            current_width = runtime_text_character_advance(context.metrics, 1, previous, current, character_scale) + current_width;
            if (*text != '\'')
            {
                flags &= ~2u;
            }
        }
        previous = *text;
        ++text;
    }
    if ((flags & 4) == 0)
    {
        ++line;
    }
    if (current_width > maximum)
    {
        maximum = current_width;
    }
    page.width = maximum;
    page.lines = line;
}
}
