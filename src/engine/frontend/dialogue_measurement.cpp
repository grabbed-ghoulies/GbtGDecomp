#include "dialogue_measurement.h"
#include "../runtime/crt_format.h"
#include <bit>

namespace gbtg
{
// Xbox 0x00059FE0
float measure_dialogue_segment(const char16_t* text, bool paragraph,
    float scale, DialogueMeasurementContext& context)
{
    float width = 0.f;
    u16 previous = 0;
    u32 flags = 0;
    context.tags.trophies.flags = 0;
    if (paragraph)
    {
        flags = 2;
    }
    if (*text != 0)
    {
        do
        {
            if ((flags & 5) != 0)
            {
                return width;
            }
            const u16 current = *text;
            if (current == '{')
            {
                text = process_dialogue_tag(text, context.tags);
                flags = context.tags.trophies.flags;
                context.tags.trophies.flags = 0;
                if (flags != 0)
                {
                    if ((flags & 8) != 0)
                    {
                        const float icon_width = static_cast<float>(read_u16(context.tags.trophies.resources.icon_resource, 8));
                        if (scale != 0.f)
                        {
                            width = icon_width * scale + width;
                        }
                        else
                        {
                            width = icon_width * context.drawing_scale + width;
                        }
                    }
                    if ((flags & 0x10) != 0)
                    {
                        if (scale != 0.f)
                        {
                            width = scale * 50.f + width; // DAT_0032B1E0
                        }
                        else
                        {
                            width = context.drawing_scale * 50.f + width;
                        }
                    }
                    char16_t* replacement;
                    if ((flags & 0x40) != 0)
                    {
                        dialogue_total_first(replacement, context.wide_scratch, context.narrow_scratch, context.first_count, context.first_additional);
                        const float advance = measure_dialogue_segment(replacement, false, scale, context);
                        width = static_cast<float>(static_cast<long double>(advance) + width);
                    }
                    if ((flags & 0x80) != 0)
                    {
                        dialogue_second_count(replacement, context.wide_scratch, context.narrow_scratch, context.second_count);
                        const float advance = measure_dialogue_segment(replacement, false, scale, context);
                        width = static_cast<float>(static_cast<long double>(advance) + width);
                    }
                    if ((flags & 0x100) != 0)
                    {
                        dialogue_total_third(replacement, context.wide_scratch, context.narrow_scratch, context.third_count, context.third_additional);
                        const float advance = measure_dialogue_segment(replacement, false, scale, context);
                        width = static_cast<float>(static_cast<long double>(advance) + width);
                    }
                    if ((flags & 0x20) != 0)
                    {
                        dialogue_bit_count(replacement, context.wide_scratch, context.narrow_scratch, context.bits);
                        const float advance = measure_dialogue_segment(replacement, false, scale, context);
                        width = static_cast<float>(static_cast<long double>(advance) + width);
                    }
                    if ((flags & 0x200) != 0)
                    {
                        dialogue_elapsed_time(replacement, context.wide_scratch, context.elapsed, context.elapsed_additional, context.localtext);
                        const float advance = measure_dialogue_segment(replacement, false, scale, context);
                        width = static_cast<float>(static_cast<long double>(advance) + width);
                    }
                    if ((flags & 0x800) != 0)
                    {
                        context.wide_scratch[0] = 0;
                        scene_detail::crt_format_wide_string(context.wide_scratch, u"%d", context.tags.trophies.thresholds[0]);
                        replacement = context.wide_scratch;
                        const float advance = measure_dialogue_segment(replacement, false, scale, context);
                        width = static_cast<float>(static_cast<long double>(advance) + width);
                    }
                    if ((flags & 0x1000) != 0)
                    {
                        context.wide_scratch[0] = 0;
                        scene_detail::crt_format_wide_string(context.wide_scratch, u"%d", context.tags.trophies.thresholds[1]);
                        replacement = context.wide_scratch;
                        const float advance = measure_dialogue_segment(replacement, false, scale, context);
                        width = static_cast<float>(static_cast<long double>(advance) + width);
                    }
                    if ((flags & 0x2000) != 0)
                    {
                        context.wide_scratch[0] = 0;
                        scene_detail::crt_format_wide_string(context.wide_scratch, u"%d", context.tags.trophies.thresholds[2]);
                        replacement = context.wide_scratch;
                        const float advance = measure_dialogue_segment(replacement, false, scale, context);
                        width = static_cast<float>(static_cast<long double>(advance) + width);
                    }
                    if ((flags & 0x4000) != 0)
                    {
                        context.wide_scratch[0] = 0;
                        scene_detail::crt_format_wide_string(context.wide_scratch, u"%d", context.tags.trophies.thresholds[3]);
                        replacement = context.wide_scratch;
                        const float advance = measure_dialogue_segment(replacement, false, scale, context);
                        width = static_cast<float>(static_cast<long double>(advance) + width);
                    }
                    if ((flags & 0x80000) != 0)
                    {
                        context.wide_scratch[0] = 0;
                        append_dialogue_seconds(context.wide_scratch, static_cast<float>(context.tags.trophies.thresholds[0]), 1, context.localtext);
                        replacement = context.wide_scratch;
                        const float advance = measure_dialogue_segment(replacement, false, scale, context);
                        width = static_cast<float>(static_cast<long double>(advance) + width);
                    }
                    if ((flags & 0x100000) != 0)
                    {
                        context.wide_scratch[0] = 0;
                        append_dialogue_seconds(context.wide_scratch, static_cast<float>(context.tags.trophies.thresholds[1]), 1, context.localtext);
                        replacement = context.wide_scratch;
                        const float advance = measure_dialogue_segment(replacement, false, scale, context);
                        width = static_cast<float>(static_cast<long double>(advance) + width);
                    }
                    if ((flags & 0x200000) != 0)
                    {
                        context.wide_scratch[0] = 0;
                        append_dialogue_seconds(context.wide_scratch, static_cast<float>(context.tags.trophies.thresholds[2]), 1, context.localtext);
                        replacement = context.wide_scratch;
                        const float advance = measure_dialogue_segment(replacement, false, scale, context);
                        width = static_cast<float>(static_cast<long double>(advance) + width);
                    }
                    if ((flags & 0x400000) != 0)
                    {
                        context.wide_scratch[0] = 0;
                        append_dialogue_seconds(context.wide_scratch, static_cast<float>(context.tags.trophies.thresholds[3]), 1, context.localtext);
                        replacement = context.wide_scratch;
                        const float advance = measure_dialogue_segment(replacement, false, scale, context);
                        width = static_cast<float>(static_cast<long double>(advance) + width);
                    }
                    if ((flags & 0x8000) != 0)
                    {
                        context.wide_scratch[0] = 0;
                        scene_detail::crt_format_wide_string(context.wide_scratch, u"%d", context.tags.high_score);
                        replacement = context.wide_scratch;
                        const float advance = measure_dialogue_segment(replacement, false, scale, context);
                        width = static_cast<float>(static_cast<long double>(advance) + width);
                    }
                    if ((flags & 0x10000) != 0)
                    {
                        context.wide_scratch[0] = 0;
                        append_dialogue_seconds(context.wide_scratch, static_cast<float>(context.tags.high_score), 1, context.localtext);
                        replacement = context.wide_scratch;
                        const float advance = measure_dialogue_segment(replacement, false, scale, context);
                        width = static_cast<float>(static_cast<long double>(advance) + width);
                    }
                    if ((flags & 0x20000) != 0)
                    {
                        dialogue_final_score(replacement, context.wide_scratch, context.tags.final_scores.data(), context.tags.selected_score);
                        const float advance = measure_dialogue_segment(replacement, false, scale, context);
                        width = static_cast<float>(static_cast<long double>(advance) + width);
                    }
                    if ((flags & 0x40000) != 0)
                    {
                        dialogue_final_time(replacement, context.wide_scratch, context.tags.final_scores.data(), context.tags.selected_score, context.localtext);
                        const float advance = measure_dialogue_segment(replacement, false, scale, context);
                        width = static_cast<float>(static_cast<long double>(advance) + width);
                    }
                }
            }
            else if (current == ' ')
            {
                if (scale != 0.f)
                {
                    width = runtime_text_space_advance(*context.metrics.fonts[1], scale, context.metrics.scale_x) + width;
                }
                else
                {
                    width = runtime_text_space_advance(*context.metrics.fonts[1], 0.f, context.metrics.scale_x) + width;
                }
            }
            else if ((flags & 2) != 0 && context.tags.state.no_paragraph == 0 && context.paragraph_disabled == 0)
            {
                float character_scale;
                if (scale != 0.f)
                {
                    character_scale = scale * 2.f;
                }
                else
                {
                    character_scale = context.initial_scale * context.drawing_scale;
                }
                width = runtime_text_character_advance(context.metrics, 1, previous, current, character_scale) + width;
                if (*text != '\'')
                {
                    flags &= ~2u;
                }
            }
            else
            {
                float character_scale = scale;
                if (scale == 0.f)
                {
                    character_scale = 0.f;
                }
                width = runtime_text_character_advance(context.metrics, 1, previous, current, character_scale) + width;
            }
            previous = *text;
            ++text;
        }
        while (*text != 0);
    }
    return width;
}
}
