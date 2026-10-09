#include "dialogue_widget_state.h"
#include "dialogue_widget_parameters.h"
#include "../entities/ghouly_spawn_record.h"
#include <bit>

namespace gbtg
{
static_assert(sizeof(RuntimeDialogueWidgetLabel) == 0x84);

// Xbox 0x001428F0
float measure_dialogue_widget_width(const char* resource, const u8* parameters,
    float icon_scale, float text_scale, const char* group, char16_t* scratch,
    RuntimeDialogueWidgetContext& context)
{
    float maximum = 0.f;
    *scratch = 0;
    std::span<const u8> acquired;
    if (read_u32({reinterpret_cast<const u8*>(resource), 4}, 0) == 0x5F646961u)
    {
        acquired = acquire_animation_resource(context.text.registry, resource);
        if (acquired.data() == nullptr)
        {
            resource_error(context.text.error, 2);
        }
    }
    i32 index = 0;
    i32 count = 0;
    do
    {
        if (read_u32({parameters + 0x40 + index * 0x48, 4}, 0) == 0xFFFFFFFFu)
        {
            count = index;
            break;
        }
        ++index;
    }
    while (index < 10);
    if (0 < count)
    {
        const char* key = reinterpret_cast<const char*>(parameters + 0x44);
        do
        {
            const char* cursor = key;
            do
            {
                ++cursor;
            }
            while (cursor[-1] != '\0');
            if (cursor != key + 1)
            {
                lookup_borrowed_localtext(acquired, group, key, scratch, context.text);
                const float width = runtime_text_line_width(context.metrics, 0, scratch, text_scale);
                if (width > maximum)
                {
                    maximum = width;
                }
            }
            key += 0x48;
            --count;
        }
        while (count != 0);
    }
    release_animation_resource(context.text.registry, acquired);
    return (icon_scale * 32.f + maximum) + 20.f;
}

// Xbox 0x001421C0
void position_dialogue_widget(RuntimeDialogueWidget& widget, const std::array<float, 2>& anchor)
{
    widget.positions[0] = anchor;
    if (widget.layout_mode == 0)
    {
        widget.positions[0] = anchor;
        widget.positions[1][0] = widget.positions[0][0] + widget.row_step[0];
        widget.positions[1][1] = widget.positions[0][1] + widget.row_step[1];
        widget.positions[2][0] = widget.positions[1][0] + widget.row_step[0];
        widget.positions[2][1] = widget.positions[1][1] + widget.row_step[1];
        widget.positions[3][0] = widget.positions[2][0] + widget.row_step[0];
        widget.positions[3][1] = widget.positions[2][1] + widget.row_step[1];
        widget.positions[4][0] = widget.positions[3][0] + widget.row_step[0];
        widget.positions[4][1] = widget.positions[3][1] + widget.row_step[1];
        widget.positions[5][0] = widget.positions[4][0] + widget.row_step[0];
        widget.positions[5][1] = widget.positions[4][1] + widget.row_step[1];
        widget.positions[6][0] = widget.positions[5][0] + widget.row_step[0];
        widget.positions[6][1] = widget.positions[5][1] + widget.row_step[1];
        widget.positions[7][0] = widget.positions[6][0] + widget.row_step[0];
        widget.positions[7][1] = widget.positions[6][1] + widget.row_step[1];
        widget.positions[8][0] = widget.positions[7][0] + widget.row_step[0];
        widget.positions[8][1] = widget.positions[7][1] + widget.row_step[1];
        widget.positions[9][0] = widget.positions[8][0] + widget.row_step[0];
        widget.positions[9][1] = widget.positions[8][1] + widget.row_step[1];
    }
    else if (widget.layout_mode == 1)
    {
        widget.positions[0] = anchor;
        widget.positions[1][0] = widget.positions[0][0] + widget.column_step[0];
        widget.positions[1][1] = widget.positions[0][1] + widget.column_step[1];
        i32 index = 2;
        i32 remaining = 4;
        do
        {
            widget.positions[index][0] = widget.positions[index - 2][0] + widget.row_step[0];
            widget.positions[index][1] = widget.positions[index - 2][1] + widget.row_step[1];
            index += 2;
            --remaining;
        }
        while (remaining != 0);
        index = 3;
        remaining = 4;
        do
        {
            widget.positions[index][0] = widget.positions[index - 2][0] + widget.row_step[0];
            widget.positions[index][1] = widget.positions[index - 2][1] + widget.row_step[1];
            index += 2;
            --remaining;
        }
        while (remaining != 0);
        return;
    }
}

// Xbox 0x00142AF0
void layout_dialogue_widget(RuntimeDialogueWidget& widget, RuntimeDialogueWidgetContext& context)
{
    i32 index = 0;
    if (0 < widget.count)
    {
        i32 offset = 0;
        do
        {
            const char* key = reinterpret_cast<const char*>(widget.current_set + offset + 0x44);
            const char* cursor = key;
            do
            {
                ++cursor;
            }
            while (cursor[-1] != '\0');
            if (cursor == key + 1)
            {
                widget.labels[index].text[0] = 0;
            }
            else
            {
                lookup_borrowed_localtext(widget.text_resource, widget.group.data(), key,
                    widget.labels[index].text.data(), context.text);
            }
            ++index;
            offset += 0x48;
        }
        while (index < widget.count);
    }
    index = 0;
    if (0 < widget.count)
    {
        do
        {
            widget.labels[index].id = create_dialogue_text_record(*widget.pool,
                widget.labels[index].text.data(),
                widget.positions[index][0] + widget.origin[0],
                widget.positions[index][1] + widget.origin[1], widget.scale, widget.scale,
                3.f, 0, 0, 1, context.metrics, context.viewport_width,
                context.next_id, context.random_state);
            ++index;
        }
        while (index < widget.count);
    }
}

// Xbox 0x00142C20
void set_dialogue_widget_mode(RuntimeDialogueWidget& widget, i32 mode,
    RuntimeDialogueWidgetContext& context)
{
    if (widget.mode != mode)
    {
        while (widget.mode = mode, mode != 1)
        {
            if (mode != 3)
            {
                return;
            }
            widget.progress = widget.duration;
            if (widget.current_set != nullptr)
            {
                if (widget.pool->allocation->cursor <= widget.pool->allocation->begin)
                {
                    return;
                }
                i32 index = 0;
                if (widget.count < 1)
                {
                    return;
                }
                do
                {
                    end_dialogue_text_record(*widget.pool, widget.labels[index].id,
                        context.metrics.fonts, context.fallback_texture, context.random_state);
                    ++index;
                }
                while (index < widget.count);
                return;
            }
            mode = 1;
            if (widget.mode == 1)
            {
                return;
            }
        }
        if (widget.requested_set == nullptr)
        {
            widget.old_set = nullptr;
            widget.current_set = nullptr;
        }
        else
        {
            copy_dialogue_widget_parameters(widget.old_parameters, widget.current_set);
            copy_dialogue_widget_parameters(widget.new_parameters, widget.requested_set);
            widget.old_set = widget.old_parameters.data();
            widget.current_set = widget.new_parameters.data();
        }
        widget.requested_set = nullptr;
        widget.timer = 0.f;
        widget.count = 0;
        widget.progress = 0.f;
        if (widget.current_set != nullptr)
        {
            i32 remaining = 10;
            i32 index = 0;
            do
            {
                if (widget.icons[index].data() != nullptr)
                {
                    release_animation_resource(context.text.registry, widget.icons[index]);
                    widget.icons[index] = {};
                }
                ++index;
                --remaining;
            }
            while (remaining != 0);
            index = 0;
            i32 offset = 0;
            do
            {
                const i32 icon = std::bit_cast<i32>(read_u32({widget.current_set, 0x310}, offset + 0x40));
                if (icon == -1)
                {
                    widget.count = index;
                    break;
                }
                if (icon != 9)
                {
                    const char* name = context.icon_names[icon];
                    std::span<const u8> acquired;
                    if (read_u32({reinterpret_cast<const u8*>(name), 4}, 0) == 0x5F646961u)
                    {
                        acquired = acquire_animation_resource(context.text.registry, name);
                        if (acquired.data() == nullptr)
                        {
                            resource_error(context.text.error, 2);
                        }
                    }
                    widget.icons[index] = acquired;
                }
                ++index;
                offset += 0x48;
            }
            while (offset < 0x2D0);
            layout_dialogue_widget(widget, context);
            widget.progress = widget.duration;
        }
    }
}

// Xbox 0x00142180
void set_dialogue_widget_prompt(RuntimeDialogueWidget& widget, const u8* parameters,
    RuntimeDialogueWidgetContext& context)
{
    if (parameters != nullptr)
    {
        copy_dialogue_widget_parameters(widget.requested_parameters, parameters);
        widget.requested_set = widget.requested_parameters.data();
        set_dialogue_widget_mode(widget, 3, context);
        return;
    }
    widget.requested_set = nullptr;
    set_dialogue_widget_mode(widget, 3, context);
}
}
