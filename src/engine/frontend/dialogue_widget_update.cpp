#include "dialogue_widget_update.h"
#include "../entities/ghouly_spawn_record.h"
#include <bit>

namespace gbtg
{
// Xbox 0x00142460
u32 update_dialogue_widget(RuntimeDialogueWidget& widget,
    const RuntimeDialogueInputDevice* device,
    DialogueWidgetUpdateContext& context)
{
    if (device != nullptr)
    {
        activate_dialogue_input(context.input);
    }
    if (widget.bat != nullptr)
    {
        const auto& bat = context.bats.at(widget.bat);
        update_button_bat(bat.bat, bat.frame, bat.context);
    }
    if (widget.mode == 1)
    {
        float step;
        if (context.freeze <= 0.f)
        {
            step = context.dt * context.scene_scale;
        }
        else
        {
            step = 0.f;
        }
        if (0.f < widget.progress)
        {
            step = widget.progress - step;
            widget.progress = step;
            if (step <= 0.f)
            {
                widget.progress = 0.f;
            }
        }
        float duration = widget.duration;
        float scale = widget.icon_scale;
        if (duration != 0.f)
        {
            duration = ((widget.progress - duration) / (0.f - duration)) * scale;
            if (scale <= 0.f)
            {
                if (scale <= duration)
                {
                    scale = duration;
                    if (0.f < duration)
                    {
                        scale = 0.f;
                    }
                }
            }
            else if (duration <= scale)
            {
                scale = duration;
                if (duration < 0.f)
                {
                    scale = 0.f;
                }
            }
        }
        widget.timer = scale;
        constexpr float threshold = std::bit_cast<float>(0x3A83126Fu); // DAT_0032B1A4
        if (widget.progress <= threshold && widget.progress != threshold)
        {
            widget.timer = widget.icon_scale;
            if (widget.mode != 2)
            {
                widget.mode = 2;
            }
        }
    }
    else if (widget.mode == 2)
    {
        i32 index = 0;
        if (device != nullptr && 0 < widget.count)
        {
            i32 offset = 0;
            do
            {
                const i32 icon = std::bit_cast<i32>(read_u32({widget.current_set, 0x310}, 0x40 + offset));
                bool selected = false;
                switch (icon)
                {
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 8:
                {
                    constexpr u16 masks[]{0x0110, 0x0220, 0x0400, 0x0800, 0x1000, 0x2000, 0x0040, 0x000F, 0xC000};
                    if (runtime_input_button_edge(*device, masks[icon], context.input) != 0)
                    {
                        if (widget.sound_enabled != 0)
                        {
                            request_audio_event_voice(widget.sound_name.data(), 1, context.voices);
                        }
                        selected = true;
                    }
                    break;
                }
                case 6:
                case 7:
                {
                    std::array<float, 2> direction;
                    dialogue_input_direction(*device, direction, context.input);
                    if (direction[0] != 0.f || direction[1] != 0.f)
                    {
                        selected = true;
                    }
                    break;
                }
                }
                if (selected)
                {
                    const u32 callback = read_u32({widget.current_set, 0x310}, 0x84 + index * 0x48);
                    if (callback != 0)
                    {
                        context.callbacks.at(callback)();
                    }
                    return 1;
                }
                ++index;
                offset += 0x48;
                if (widget.count <= index)
                {
                    return 0;
                }
            }
            while (true);
        }
    }
    else if (widget.mode == 3)
    {
        float step;
        if (context.freeze <= 0.f)
        {
            step = context.dt * context.scene_scale;
        }
        else
        {
            step = 0.f;
        }
        if (0.f < widget.progress)
        {
            step = widget.progress - step;
            widget.progress = step;
            if (step <= 0.f)
            {
                widget.progress = 0.f;
            }
        }
        float scale = widget.icon_scale;
        if (widget.duration != 0.f)
        {
            const float value = (widget.progress / widget.duration) * scale;
            if (scale <= 0.f)
            {
                if (scale <= value)
                {
                    scale = value;
                    if (0.f < value)
                    {
                        scale = 0.f;
                    }
                }
            }
            else if (value <= scale)
            {
                scale = value;
                if (value < 0.f)
                {
                    scale = 0.f;
                }
            }
        }
        widget.timer = scale;
        constexpr float threshold = std::bit_cast<float>(0x3A83126Fu);
        if (scale < threshold && widget.pool->allocation->cursor <= widget.pool->allocation->begin)
        {
            set_dialogue_widget_mode(widget, 1, context.widget);
            return 0;
        }
    }
    return 0;
}
}
