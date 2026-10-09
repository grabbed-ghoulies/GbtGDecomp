#include "dialogue_sound_tags.h"
#include "dialogue_tags.h"
#include "../runtime/crt_strings.h"
#include <cstring>

namespace gbtg
{
const DialogueSoundSwapNames g_retail_dialogue_sound_swaps{
    {
        u"GDIALOGUE_BOY_BREATHLAUGH",
        u"GDIALOGUE_BOY_BUILDUP",
        u"GDIALOGUE_BOY_COWER",
        u"GDIALOGUE_BOY_EXHALE",
        u"GDIALOGUE_BOY_FRIGHT",
        u"GDIALOGUE_BOY_GENERAL",
        u"GDIALOGUE_BOY_HEREYOUARE",
        u"GDIALOGUE_BOY_HEY",
        u"GDIALOGUE_BOY_LITTLESCARED",
        u"GDIALOGUE_BOY_NERVOUSLAUGH",
        u"GDIALOGUE_BOY_OKAY",
        u"GDIALOGUE_BOY_PUZZLED",
        u"GDIALOGUE_BOY_QUESTION",
        u"GDIALOGUE_BOY_NO",
        u"GDIALOGUE_BOY_SHOCKEXHALE",
        u"GDIALOGUE_BOY_SIGH",
        u"GDIALOGUE_BOY_WOA",
        u"GDIALOGUE_BOY_YEAH"
    },
    {
        u"GDIALOGUE_GIRL_BREATHLAUGH",
        u"GDIALOGUE_GIRL_BUILDUP",
        u"GDIALOGUE_GIRL_DROPTOBUM",
        u"GDIALOGUE_GIRL_DROPTOKNEES",
        u"GDIALOGUE_GIRL_FAINT",
        u"GDIALOGUE_GIRL_MUFFLEDSPEAK",
        u"GDIALOGUE_GIRL_MUFFLEDQUESTION",
        u"GDIALOGUE_GIRL_POINT",
        u"GDIALOGUE_GIRL_YEAH"
    },
    {
        u"GDIALOGUE_GIRL_BREATHLAUGH",
        u"GDIALOGUE_GIRL_BUILDUP",
        u"GDIALOGUE_GIRL_DROPTOBUM",
        u"GDIALOGUE_GIRL_MUFFLEDQUESTION",
        u"GDIALOGUE_GIRL_FAINT",
        u"GDIALOGUE_GIRL_MUFFLEDSPEAK",
        u"GDIALOGUE_GIRL_MUFFLEDSPEAK",
        u"GDIALOGUE_GIRL_POINT",
        u"GDIALOGUE_GIRL_MUFFLEDSPEAK",
        u"GDIALOGUE_GIRL_BREATHLAUGH",
        u"GDIALOGUE_GIRL_YEAH",
        u"GDIALOGUE_GIRL_MUFFLEDQUESTION",
        u"GDIALOGUE_GIRL_MUFFLEDQUESTION",
        u"GDIALOGUE_GIRL_MUFFLEDSPEAK",
        u"GDIALOGUE_GIRL_FAINT",
        u"GDIALOGUE_GIRL_DROPTOKNEES",
        u"GDIALOGUE_GIRL_YEAH",
        u"GDIALOGUE_GIRL_YEAH"
    },
    {
        u"GDIALOGUE_BOY_BREATHLAUGH",
        u"GDIALOGUE_BOY_BUILDUP",
        u"GDIALOGUE_BOY_HEREYOUARE",
        u"GDIALOGUE_BOY_HEREYOUARE",
        u"GDIALOGUE_BOY_LITTLESCARED",
        u"GDIALOGUE_BOY_PUZZLED",
        u"GDIALOGUE_BOY_QUESTION",
        u"GDIALOGUE_BOY_YEAH",
        u"GDIALOGUE_BOY_YEAH"
    },
};

// Xbox 0x00057B00
void select_dialogue_sound_tag(const char16_t* text, u8 mode_flags, u32 scene_flags,
    DialogueSoundTagState& state, const DialogueSoundSwapNames& names)
{
    bool swapped = false;
    while (*text != 0 && *text == 0x20)
    {
        ++text;
    }
    if ((mode_flags & 8) != 0 || (scene_flags & 0x200000) != 0)
    {
        if (compare_dialogue_wide_prefix(text, u"GDIALOGUE_BOY", xapi_wide_text_length(u"GDIALOGUE_BOY")) == 0)
        {
            u32 index = 0;
            do
            {
                if (compare_dialogue_wide_prefix(text, names.boy_from[index],
                        xapi_wide_text_length(names.boy_from[index])) == 0)
                {
                    text = names.girl_to[index];
                    swapped = true;
                    goto copy;
                }
                ++index;
            }
            while (index < 18);
        }
        else if (compare_dialogue_wide_prefix(text, u"GDIALOGUE_GIRL", xapi_wide_text_length(u"GDIALOGUE_GIRL")) == 0)
        {
            u32 index = 0;
            do
            {
                if (compare_dialogue_wide_prefix(text, names.girl_from[index],
                        xapi_wide_text_length(names.girl_from[index])) == 0)
                {
                    text = names.boy_to[index];
                    swapped = true;
                    goto copy;
                }
                ++index;
            }
            while (index <= 8);
        }
    }
copy:
    if (*text != 0)
    {
        if (swapped)
        {
            const i32 length = static_cast<i32>(xapi_wide_text_length(text));
            i32 index = 0;
            if (0 < length)
            {
                do
                {
                    state.name[index] = static_cast<char>(text[index]);
                    ++index;
                }
                while (index < length);
            }
            state.name[length] = 0;
            i32 prefix = 0;
            char character;
            do
            {
                character = "XACT_SOUNDBANK_"[prefix];
                state.soundbank[prefix] = character;
                ++prefix;
            }
            while (character != 0);
            const char* source_end = state.name.data();
            do
            {
                ++source_end;
            }
            while (source_end[-1] != 0);
            char* destination_end = state.soundbank.data();
            do
            {
                ++destination_end;
            }
            while (destination_end[-1] != 0);
            std::memcpy(destination_end - 1, state.name.data(), source_end - state.name.data());
        }
        else
        {
            u32 count = 0;
            char16_t character = *text;
            do
            {
                if (character == 0x7D)
                {
                    break;
                }
                character = text[count + 1];
                ++count;
            }
            while (character != 0);
            u32 length = xapi_wide_text_length(text);
            if (static_cast<i32>(count) <= static_cast<i32>(length))
            {
                length = count;
            }
            i32 index = 0;
            if (0 < static_cast<i32>(length))
            {
                do
                {
                    state.name[index] = static_cast<char>(text[index]);
                    ++index;
                }
                while (index < static_cast<i32>(length));
            }
            state.name[length] = 0;
            i32 prefix = 0;
            char copied;
            do
            {
                copied = "XACT_SOUNDBANK_"[prefix];
                state.soundbank[prefix] = copied;
                ++prefix;
            }
            while (copied != 0);
            scene_detail::crt_append_string_count(state.soundbank.data(), state.name.data(), count);
        }
        scene_detail::crt_copy_string_count(state.cue.data(), state.soundbank.data(), 0x80);
        char* cursor = state.cue.data();
        char character = *cursor;
        while (character != 0)
        {
            character = *cursor;
            if (character >= 'a' && character <= 'z')
            {
                *cursor = character - 0x20;
            }
            ++cursor;
            character = *cursor;
        }
    }
}
}
