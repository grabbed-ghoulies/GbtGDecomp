#include "dialogue_substitutions.h"
#include "../runtime/crt_strings.h"
#include "../runtime/crt_format.h"
#include "../../gbtg/text/localtext_resources.h"
#include <bit>

namespace gbtg
{
// Xbox 0x00066180
i32 count_dialogue_bits(const DialogueBitsetView& bits)
{
    i32 count = 0;
    for (i32 index = 0; index < bits.byte_count; ++index)
    {
        i32 byte_count = 0;
        for (u8 value = bits.bytes[index]; value != 0; value = static_cast<u8>(value & (value - 1)))
        {
            ++byte_count;
        }
        count = std::bit_cast<i32>(static_cast<u32>(count) + static_cast<u32>(byte_count));
    }
    return count;
}

// Xbox 0x0005A810
void dialogue_bit_count(char16_t*& result, char16_t* wide, char* narrow,
    const DialogueBitsetView& bits)
{
    result = wide;
    wide[0] = 0;
    narrow[0] = 0;
    scene_detail::crt_integer_to_string(count_dialogue_bits(bits), narrow, 10);
    const char* cursor = narrow;
    char character;
    do
    {
        character = *cursor++;
    }
    while (character != 0);
    const i32 length = static_cast<i32>(cursor - narrow - 1);
    i32 index = 0;
    if (length > 0)
    {
        do
        {
            wide[index] = static_cast<char16_t>(static_cast<i16>(static_cast<signed char>(narrow[index])));
            ++index;
        }
        while (index < length);
    }
    wide[length] = 0;
}

// Xbox 0x0005A920
void dialogue_final_score(char16_t*& result, char16_t* wide,
    const i32* scores, i32 selected)
{
    result = wide;
    wide[0] = 0;
    if (scores[selected] == -1)
    {
        copy_xapi_wide_text(result, u"?"); // DAT_00323C34
        return;
    }
    scene_detail::crt_format_wide_string(result, u"%d", scores[selected]); // DAT_00323C38
}

// Xbox 0x0005A960
void dialogue_final_time(char16_t*& result, char16_t* wide,
    const i32* scores, i32 selected, LocalTextResourceContext& context)
{
    result = wide;
    wide[0] = 0;
    if (scores[selected] == -1)
    {
        copy_xapi_wide_text(result, u"?");
        return;
    }
    append_dialogue_seconds(result, static_cast<float>(scores[selected]), 1, context);
}

// Xbox 0x0005A880
void dialogue_elapsed_time(char16_t*& result, char16_t* wide, i32 elapsed,
    float additional, LocalTextResourceContext& context)
{
    result = wide;
    wide[0] = 0;
    const float seconds = static_cast<float>(static_cast<long double>(elapsed) + additional);
    append_dialogue_minutes(result, seconds, 1, context);
}

// Xbox 0x0005A6C0
void dialogue_total_first(char16_t*& result, char16_t* wide, char* narrow, i32 count, i32 additional)
{
    result = wide;
    wide[0] = 0;
    narrow[0] = 0;
    scene_detail::crt_integer_to_string(std::bit_cast<i32>(static_cast<u32>(count) + static_cast<u32>(additional)), narrow, 10);
    const char* cursor = narrow;
    char character;
    do
    {
        character = *cursor++;
    }
    while (character != 0);
    const i32 length = static_cast<i32>(cursor - narrow - 1);
    i32 index = 0;
    if (length > 0)
    {
        do
        {
            wide[index] = static_cast<char16_t>(static_cast<i16>(static_cast<signed char>(narrow[index])));
            ++index;
        }
        while (index < length);
    }
    wide[length] = 0;
}

// Xbox 0x0005A730
void dialogue_second_count(char16_t*& result, char16_t* wide, char* narrow, i32 count)
{
    result = wide;
    wide[0] = 0;
    narrow[0] = 0;
    scene_detail::crt_integer_to_string(count, narrow, 10);
    const char* cursor = narrow;
    char character;
    do
    {
        character = *cursor++;
    }
    while (character != 0);
    const i32 length = static_cast<i32>(cursor - narrow - 1);
    i32 index = 0;
    if (length > 0)
    {
        do
        {
            wide[index] = static_cast<char16_t>(static_cast<i16>(static_cast<signed char>(narrow[index])));
            ++index;
        }
        while (index < length);
    }
    wide[length] = 0;
}

// Xbox 0x0005A7A0
void dialogue_total_third(char16_t*& result, char16_t* wide, char* narrow, i32 count, i32 additional)
{
    result = wide;
    wide[0] = 0;
    narrow[0] = 0;
    scene_detail::crt_integer_to_string(std::bit_cast<i32>(static_cast<u32>(count) + static_cast<u32>(additional)), narrow, 10);
    const char* cursor = narrow;
    char character;
    do
    {
        character = *cursor++;
    }
    while (character != 0);
    const i32 length = static_cast<i32>(cursor - narrow - 1);
    i32 index = 0;
    if (length > 0)
    {
        do
        {
            wide[index] = static_cast<char16_t>(static_cast<i16>(static_cast<signed char>(narrow[index])));
            ++index;
        }
        while (index < length);
    }
    wide[length] = 0;
}

}
