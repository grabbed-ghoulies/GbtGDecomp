#include "dialogue_draw_context.h"
#include "gbtg/anim.h"
#include "../runtime/crt_format.h"
#include <algorithm>

namespace gbtg
{
// Xbox 0x00058C30
void draw_runtime_dialogue_page(DialoguePageLayout& page, float fade,
    RuntimeDialogueDrawContext& c)
{
    auto& s = c.state;
    auto& m = c.dialogue.measurement;
    auto& tags = m.tags.state;
    s.fade = fade;
    if (fade == 0.f)
    {
        return;
    }
    float font_scale = 1.f;
    const float font_cell = static_cast<float>(c.metrics.fonts[1]->cell);
    if (font_cell != 64.f && font_cell == 32.f)
    {
        font_scale = 2.f;
    }
    const float wrap = c.dialogue.wrap_width;
    const float time = c.dialogue.state.mode == 1 ? 0.f : c.dialogue.state.elapsed;
    const char16_t* text = page.text;
    s.lines = page.lines;
    s.width = page.width * fade;
    s.scale_x = tags.scale_x * font_scale * fade;
    s.scale_y = tags.scale_y * font_scale * fade;
    const float drop = static_cast<float>(c.metrics.fonts[1]->drop);
    float line_scale = s.scale_y;
    if (line_scale == 0.f)
    {
        line_scale = c.metrics.scale_y;
    }
    if (tags.tab == 0)
    {
        s.line_step = drop * line_scale * c.ordinary_line_scale;
    }
    else
    {
        s.line_step = drop * line_scale * c.tab_line_scale;
    }
    s.space = runtime_text_space_advance(*c.metrics.fonts[1], s.scale_x, c.metrics.scale_x);
    s.paragraph_scale = 2.f;
    reset_dialogue_font(1, c);
    set_dialogue_font_scale(s.scale_x, s.scale_y, c);
    s.shadow_x = s.shadow_y = fade * 5.f;
    c.metrics.spacing = 2.f;
    if (c.dialogue.settings.characters == 0)
    {
        s.random = static_cast<u32>(c.counter()) - c.counter_baseline;
    }
    else
    {
        s.random = static_cast<u32>(c.dialogue.settings.characters);
    }
    s.origin_x = (static_cast<float>(c.width) - s.width) * .5f;
    s.origin_y = (static_cast<float>(c.height) - static_cast<float>(s.lines) * s.line_step) * .5f
        + s.line_step * .5f;
    float y = s.origin_y;
    if (tags.tab != 0)
    {
        y = 50.f;
    }
    const float origin = s.origin_x;
    initialize_dialogue_orthographic(c);
    c.current_affine = c.affine.data();
    std::fill_n(c.current_affine, 24, 0.f);
    c.current_affine[0] = c.current_affine[5] = c.current_affine[10] = 1.f;
    float* scratch = c.current_affine + 12;
    scratch[0] = scratch[5] = scratch[10] = 1.f;
    scratch[3] = static_cast<float>(c.width) * .5f;
    scratch[7] = static_cast<float>(c.height) * .5f;
    multiply_affine_matrices(c.current_affine, c.current_affine, scratch);
    std::fill_n(scratch, 12, 0.f);
    scratch[0] = scratch[5] = fade;
    scratch[10] = 1.f;
    multiply_affine_matrices(c.current_affine, c.current_affine, scratch);
    std::fill_n(scratch, 12, 0.f);
    scratch[0] = scratch[5] = scratch[10] = 1.f;
    scratch[3] = static_cast<float>(-static_cast<i32>(c.width)) * .5f;
    scratch[7] = static_cast<float>(-static_cast<i32>(c.height)) * .5f;
    multiply_affine_matrices(c.current_affine, c.current_affine, scratch);
    u32 flags = 0;
    u16 previous = 0;
    m.tags.trophies.flags = 0;
    i32 line = 0;
    if (tags.no_paragraph == 0)
    {
        flags = 2;
    }
    i32 alignment = c.dialogue.settings.initial_alignment;
    float x = dialogue_line_position(alignment, 0, origin, page, c);
    float boundary = x + wrap;
    tags.alignment = alignment;
    while (*text != 0 && (flags & 1) == 0 && line < c.dialogue.settings.line_limit)
    {
        float next_x = x;
        i32 next_alignment = alignment;
        if (*text == '{')
        {
            text = process_dialogue_tag(text, m.tags);
            u32 token_flags = m.tags.trophies.flags;
            next_alignment = tags.alignment;
            m.tags.trophies.flags = 0;
            if (alignment != tags.alignment)
            {
                next_x = dialogue_line_position(tags.alignment, line, origin, page, c);
            }
            flags = token_flags;
            if (token_flags != 0)
            {
                if ((token_flags & 4) != 0)
                {
                    ++line;
                    next_x = dialogue_line_position(next_alignment, line, origin, page, c);
                    y += s.line_step;
                    boundary = next_x + wrap;
                    token_flags &= ~4u;
                    flags = token_flags;
                }
                if ((token_flags & 8) != 0)
                {
                    const float icon_y = y - static_cast<float>(read_u16(m.tags.trophies.resources.icon_resource, 10)) * s.scale_y * .25f;
                    const u8 frame = dialogue_icon_frame(time, c);
                    draw_dialogue_icon(next_x, icon_y, s.scale_x, s.scale_y, frame, c);
                }
                if ((token_flags & 0x10) != 0)
                {
                    next_x = s.scale_x * 50.f + next_x;
                }
                char16_t* replacement;
                if ((token_flags & 0x40) != 0)
                {
                    dialogue_total_first(replacement, m.wide_scratch, m.narrow_scratch, m.first_count, m.first_additional);
                    draw_runtime_dialogue_text(replacement, next_x, y, 0, nullptr, 0.f, 0.f, 0.f, 1, c);
                }
                if ((token_flags & 0x80) != 0)
                {
                    dialogue_second_count(replacement, m.wide_scratch, m.narrow_scratch, m.second_count);
                    draw_runtime_dialogue_text(replacement, next_x, y, 0, nullptr, 0.f, 0.f, 0.f, 1, c);
                }
                if ((token_flags & 0x100) != 0)
                {
                    dialogue_total_third(replacement, m.wide_scratch, m.narrow_scratch, m.third_count, m.third_additional);
                    draw_runtime_dialogue_text(replacement, next_x, y, 0, nullptr, 0.f, 0.f, 0.f, 1, c);
                }
                if ((token_flags & 0x20) != 0)
                {
                    dialogue_bit_count(replacement, m.wide_scratch, m.narrow_scratch, m.bits);
                    draw_runtime_dialogue_text(replacement, next_x, y, 0, nullptr, 0.f, 0.f, 0.f, 1, c);
                }
                if ((token_flags & 0x200) != 0)
                {
                    dialogue_elapsed_time(replacement, m.wide_scratch, m.elapsed, m.elapsed_additional, m.localtext);
                    draw_runtime_dialogue_text(replacement, next_x, y, 0, nullptr, 0.f, 0.f, 0.f, 1, c);
                }
                for (u32 index = 0; index != 4; ++index)
                {
                    if ((token_flags & (0x800u << index)) != 0)
                    {
                        m.wide_scratch[0] = 0;
                        scene_detail::crt_format_wide_string(m.wide_scratch, u"%d", m.tags.trophies.thresholds[index]);
                        draw_runtime_dialogue_text(m.wide_scratch, next_x, y, 0, nullptr, 0.f, 0.f, 0.f, 1, c);
                    }
                }
                for (u32 index = 0; index != 4; ++index)
                {
                    if ((token_flags & (0x80000u << index)) != 0)
                    {
                        m.wide_scratch[0] = 0;
                        append_dialogue_seconds(m.wide_scratch, static_cast<float>(m.tags.trophies.thresholds[index]), 1, m.localtext);
                        draw_runtime_dialogue_text(m.wide_scratch, next_x, y, 0, nullptr, 0.f, 0.f, 0.f, 1, c);
                    }
                }
                if ((token_flags & 0x8000) != 0)
                {
                    // 0x0005A8C0, inline: the run's score (DAT_004E18F0) through swprintf("%d") into DAT_00507E78.
                    m.wide_scratch[0] = 0;
                    scene_detail::crt_format_wide_string(m.wide_scratch, u"%d", m.tags.high_score);
                    draw_runtime_dialogue_text(m.wide_scratch, next_x, y, 0, nullptr, 0.f, 0.f, 0.f, 1, c);
                }
                if ((token_flags & 0x10000) != 0)
                {
                    // 0x0005A8F0, inline: the run's score as seconds (0x000FB860) into DAT_00507E78.
                    m.wide_scratch[0] = 0;
                    append_dialogue_seconds(m.wide_scratch, static_cast<float>(m.tags.high_score), 1, m.localtext);
                    draw_runtime_dialogue_text(m.wide_scratch, next_x, y, 0, nullptr, 0.f, 0.f, 0.f, 1, c);
                }
                if ((token_flags & 0x20000) != 0)
                {
                    dialogue_final_score(replacement, m.wide_scratch, m.tags.final_scores.data(), m.tags.selected_score);
                    draw_runtime_dialogue_text(replacement, next_x, y, 0, nullptr, 0.f, 0.f, 0.f, 1, c);
                }
                if ((token_flags & 0x40000) != 0)
                {
                    dialogue_final_time(replacement, m.wide_scratch, m.tags.final_scores.data(), m.tags.selected_score, m.localtext);
                    draw_runtime_dialogue_text(replacement, next_x, y, 0, nullptr, 0.f, 0.f, 0.f, 1, c);
                }
            }
        }
        else if (*text == ' ')
        {
            const float word = runtime_text_word_advance(c.metrics, 1, text + 1, '{', 0.f);
            const float end = word + s.space + x;
            if (end <= boundary)
            {
                next_x = s.space + x;
            }
            else
            {
                ++line;
                next_x = dialogue_line_position(alignment, line, origin, page, c);
                y += s.line_step;
                boundary = next_x + boundary;
                flags &= ~4u;
            }
        }
        else
        {
            const bool paragraph = (flags & 2) != 0 && tags.no_paragraph == 0 && m.paragraph_disabled == 0;
            float offset = 0.f;
            if (paragraph)
            {
                const float paragraph_y = s.paragraph_scale * s.scale_y;
                set_dialogue_font_scale(s.paragraph_scale * s.scale_x, paragraph_y, c);
                offset = s.paragraph_offset * s.scale_y;
                if (*text == '\'')
                {
                    offset -= paragraph_y * 2.5f;
                }
                y -= offset;
            }
            draw_runtime_dialogue_glyph(*text, next_x, y, s.wave_speed, s.wave_height, time, 0, previous, 1, c);
            if (paragraph)
            {
                y += offset;
                set_dialogue_font_scale(s.scale_x, s.scale_y, c);
                if (*text != '\'')
                {
                    flags &= ~2u;
                }
            }
        }
        previous = *text++;
        x = next_x;
        alignment = next_alignment;
    }
    --c.override_count;
    s.active = 0;
    if (c.override_count == 0)
    {
        c.fog_disabled = 0;
    }
    page.next_text = text;
}
}
