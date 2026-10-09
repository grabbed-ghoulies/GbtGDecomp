#include "crt_strings.h"
#include <cstring>
#include <cstdint>

namespace gbtg::scene_detail
{
// Xbox 0x00167B2C
void crt_unsigned_to_string(u32 value, char* output, u32 radix, i32 negative)
{
    char* begin = output;
    if (negative != 0)
    {
        *output++ = '-';
        value = 0u - value;
        begin = output;
    }
    char* end;
    do
    {
        end = output;
        const u32 digit = value % radix;
        value /= radix;
        if (digit < 10)
        {
            *output = static_cast<char>(digit + '0');
        }
        else
        {
            *output = static_cast<char>(digit + 'W');
        }
        ++output;
    }
    while (value != 0);
    *output = 0;
    i32 left = 0;
    i32 right = static_cast<i32>(end - begin);
    do
    {
        const char digit = begin[right];
        begin[right] = begin[left];
        --right;
        begin[left] = digit;
        ++left;
    }
    while (left < right);
}

// Xbox 0x00167B6A
char* crt_integer_to_string(i32 value, char* output, i32 radix)
{
    if (radix == 10 && value < 0)
    {
        crt_unsigned_to_string(static_cast<u32>(value), output, 10, 1);
    }
    else
    {
        crt_unsigned_to_string(static_cast<u32>(value), output, static_cast<u32>(radix), 0);
    }
    return output;
}
static u32 string_word(const char* source)
{
    u32 word;
    std::memcpy(&word, source, 4);
    return word;
}
static void store_string_word(char* destination, u32 word)
{
    std::memcpy(destination, &word, 4);
}

// Xbox 0x001665E0
char* crt_copy_string_count(char* destination, const char* source, u32 count)
{
    if (count == 0)
    {
        return destination;
    }
    char* output = destination;
    u32 words;
    u32 word;
    char character;
    if ((reinterpret_cast<std::uintptr_t>(source) & 3) != 0)
    {
        while (true)
        {
            character = *source++;
            *output++ = character;
            --count;
            if (count == 0)
            {
                return destination;
            }
            if (character == 0)
            {
                goto align_padding;
            }
            if ((reinterpret_cast<std::uintptr_t>(source) & 3) == 0)
            {
                break;
            }
        }
    }
    words = count >> 2;
    if (words != 0)
    {
        do
        {
            word = string_word(source);
            source += 4;
            if (((word ^ 0xFFFFFFFFu ^ (word + 0x7EFEFEFFu)) & 0x81010100u) != 0)
            {
                if ((word & 0xFF) == 0)
                {
                    store_string_word(output, 0);
                    goto remaining_padding_words;
                }
                if ((word & 0xFF00) == 0)
                {
                    store_string_word(output, word & 0xFF);
                    goto remaining_padding_words;
                }
                if ((word & 0xFF0000) == 0)
                {
                    store_string_word(output, word & 0xFFFF);
                    goto remaining_padding_words;
                }
                if ((word & 0xFF000000) == 0)
                {
                    store_string_word(output, word);
                    goto remaining_padding_words;
                }
            }
            store_string_word(output, word);
            output += 4;
            --words;
        }
        while (words != 0);
        count &= 3;
        if (count == 0)
        {
            return destination;
        }
    }
    do
    {
        character = *source++;
        *output++ = character;
        if (character == 0)
        {
            while (--count != 0)
            {
                *output++ = character;
            }
            return destination;
        }
        --count;
    }
    while (count != 0);
    return destination;

align_padding:
    do
    {
        if ((reinterpret_cast<std::uintptr_t>(output) & 3) == 0)
        {
            words = count >> 2;
            if (words == 0)
            {
                goto padding_tail;
            }
            goto padding_words;
        }
        *output++ = 0;
        --count;
    }
    while (count != 0);
    return destination;
remaining_padding_words:
    while (true)
    {
        --words;
        output += 4;
        if (words == 0)
        {
            break;
        }
        store_string_word(output, 0);
    }
    goto padding_tail;
padding_words:
    do
    {
        store_string_word(output, 0);
        output += 4;
        --words;
    }
    while (words != 0);
padding_tail:
    count &= 3;
    if (count != 0)
    {
        do
        {
            *output++ = 0;
            --count;
        }
        while (count != 0);
    }
    return destination;
}

// Xbox 0x001664A0
char* crt_append_string_count(char* destination, const char* source, u32 count)
{
    if (count == 0)
    {
        return destination;
    }
    char* output = destination;
    u32 word;
    u32 words;
    while ((reinterpret_cast<std::uintptr_t>(output) & 3) != 0)
    {
        if (*output++ == 0)
        {
            --output;
            goto append;
        }
    }
    while (true)
    {
        do
        {
            word = string_word(output);
            output += 4;
        }
        while (((word ^ 0xFFFFFFFFu ^ (word + 0x7EFEFEFFu)) & 0x81010100u) == 0);
        if ((word & 0xFF) == 0)
        {
            output -= 4;
            break;
        }
        if ((word & 0xFF00) == 0)
        {
            output -= 3;
            break;
        }
        if ((word & 0xFF0000) == 0)
        {
            output -= 2;
            break;
        }
        if ((word & 0xFF000000) == 0)
        {
            --output;
            break;
        }
    }
append:
    if ((reinterpret_cast<std::uintptr_t>(source) & 3) != 0)
    {
        do
        {
            const char character = *source++;
            if (character == 0)
            {
                *output = 0;
                return destination;
            }
            *output++ = character;
            --count;
            if (count == 0)
            {
                *output = 0;
                return destination;
            }
        }
        while ((reinterpret_cast<std::uintptr_t>(source) & 3) != 0);
    }
    words = count >> 2;
    do
    {
        if (words == 0)
        {
            for (u32 remaining = count & 3; remaining != 0; --remaining)
            {
                const char character = *source++;
                *output++ = character;
                if (character == 0)
                {
                    return destination;
                }
            }
            *output = 0;
            return destination;
        }
        word = string_word(source);
        source += 4;
        if (((word ^ 0xFFFFFFFFu ^ (word + 0x7EFEFEFFu)) & 0x81010100u) != 0)
        {
            if ((word & 0xFF) == 0)
            {
                *output = 0;
                return destination;
            }
            if ((word & 0xFF00) == 0)
            {
                const u16 half = static_cast<u16>(word);
                std::memcpy(output, &half, 2);
                return destination;
            }
            if ((word & 0xFF0000) == 0)
            {
                const u16 half = static_cast<u16>(word);
                std::memcpy(output, &half, 2);
                output[2] = 0;
                return destination;
            }
            if ((word & 0xFF000000) == 0)
            {
                store_string_word(output, word);
                return destination;
            }
        }
        store_string_word(output, word);
        output += 4;
        --words;
    }
    while (true);
}
}
