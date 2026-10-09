#include "dialogue_tag_actions.h"
#include "../runtime/crt_decimal.h"

namespace gbtg
{
// Xbox 0x000FB810
double dialogue_wide_to_double(const char16_t* text, std::array<char, 64>& scratch)
{
    i32 length = static_cast<i32>(xapi_wide_text_length(text));
    if (length > 0x3E)
    {
        length = 0x3F;
    }
    i32 index = 0;
    if (0 < length)
    {
        do
        {
            scratch[index] = static_cast<char>(text[index]);
            ++index;
        }
        while (index < length);
    }
    scratch[length] = 0;
    return scene_detail::crt_ascii_to_double(scratch.data());
}

// Xbox 0x00056F80
void execute_dialogue_token(i32 token, const char16_t* arguments, DialogueTagContext& context)
{
    auto& state = context.state;
    auto& trophies = context.trophies;
    u32& flags = trophies.flags;
    if (token < 0x12)
    {
        select_dialogue_speaker(trophies.resources, token, context.mode_flags, context.scene_flags,
            trophies.names, trophies.registry, trophies.error);
        return;
    }
    if (token < 0x37 || 0x59 < token)
    {
        goto actions;
    }
    else
    {
        if (0x4A < token && token < 0x50)
        {
            select_dialogue_icon(trophies.resources, token, 1, trophies.name_buffer,
                trophies.names, trophies.registry, trophies.error);
            flags |= 8;
            mark_dialogue_pause_icon(token, flags);
            return;
        }
        u8 group;
        if (token < 0x38 || 0x48 < token)
        {
            if (token < 0x52 || 0x55 < token)
            {
                if (token < 0x58 || 0x58 < token)
                {
                    goto actions;
                }
                group = 3;
            }
            else
            {
                group = 2;
            }
        }
        else
        {
            group = 0;
        }
        select_dialogue_icon(trophies.resources, token, group, trophies.name_buffer,
            trophies.names, trophies.registry, trophies.error);
        flags |= 8;
        return;
    }
actions:
    switch (token)
    {
    case 0x13:
        state.alignment = 0;
        return;
    case 0x14:
        state.alignment = 2;
        return;
    case 0x15:
        state.alignment = 1;
        return;
    case 0x19:
        flags |= 0x1;
        return;
    case 0x1A:
        flags |= 0x6;
        return;
    case 0x1C:
        flags |= 0x4;
        return;
    case 0x20:
        flags |= 0x400;
        return;
    case 0x25:
        flags |= 0x10;
        return;
    case 0x27:
        flags |= 0x800;
        return;
    case 0x28:
        flags |= 0x1000;
        return;
    case 0x29:
        flags |= 0x2000;
        return;
    case 0x2A:
        flags |= 0x4000;
        return;
    case 0x2B:
        flags |= 0x80000;
        return;
    case 0x2C:
        flags |= 0x100000;
        return;
    case 0x2D:
        flags |= 0x200000;
        return;
    case 0x2E:
        flags |= 0x400000;
        return;
    case 0x2F:
        flags |= 0x8000;
        return;
    case 0x30:
        flags |= 0x10000;
        return;
    case 0x31:
        flags |= 0x40000;
        return;
    case 0x32:
        flags |= 0x20000;
        return;
    case 0x1D:
        flags |= 4;
        state.alignment = 0;
        return;
    case 0x1E:
        flags |= 4;
        state.alignment = 1;
        return;
    case 0x1F:
        flags |= 4;
        state.alignment = 2;
        return;
    case 0x1B:
        state.no_paragraph = 1;
        return;
    case 0x21:
        state.heads = 0;
        return;
    case 0x22:
        state.buttons = 0;
        return;
    case 0x23:
        state.time = static_cast<float>(dialogue_wide_to_double(arguments, state.numeric_scratch));
        return;
    case 0x24:
    {
        const float scale = static_cast<float>(dialogue_wide_to_double(arguments, state.numeric_scratch));
        state.scale_x = scale;
        state.scale_y = scale;
        return;
    }
    case 0x26:
        state.tab = 1;
        return;
    case 0x33:
        select_final_dialogue_trophy(context.final_scores, context.selected_score, trophies);
        return;
    case 0x34:
        select_high_dialogue_trophy(context.high_score, trophies);
        return;
    case 0x35:
        state.invoke_map = 1;
        return;
    case 0x5B:
        select_dialogue_sound_tag(arguments, context.mode_flags, context.scene_flags, context.sound, context.sound_names);
        return;
    }
}

// Xbox 0x00056EA0
const char16_t* process_dialogue_tag(const char16_t* text, DialogueTagContext& context)
{
    i32 index = 1;
    char16_t character = text[1];
    while (character != 0 && character != 0x20 && character != 0x7D)
    {
        character = text[index + 1];
        ++index;
    }
    const i32 token = find_dialogue_token(text + 1, context.tokens);
    if (token != 0x5C)
    {
        execute_dialogue_token(token, text + index, context);
    }
    character = text[index];
    while (character != 0 && character != 0x7D)
    {
        character = text[index + 1];
        ++index;
    }
    return text + index;
}
}
