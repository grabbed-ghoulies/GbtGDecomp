#include "crt_decimal.h"
#include "crt_runtime.h"

namespace gbtg::scene_detail
{
static u32 decimal_digit_type(u8 character)
{
    if (g_crt_mb_cur_max < 2)
    {
        return static_cast<u8>(g_crt_pctype[character]) & 4;
    }
    return crt_character_type(character, 4);
}

// Xbox 0x0016E7CA
u32 crt_parse_extended(CrtMantissa& output, const char*& end, const char* text,
    i32 preserve_tail, i32 scale, i32 decimal_position, i32 implicit_exponent)
{
    std::array<signed char, 25> digits;
    u32 count = 0;
    i32 exponent_sign = 1;
    u16 sign = 0;
    i32 adjustment = 0;
    i32 magnitude = 0;
    i32 saw_decimal = 0;
    i32 saw_exponent = 0;
    i32 saw_number = 0;
    u32 status = 0;
    const char* cursor = text;
    while (*cursor == ' ' || *cursor == '\t' || *cursor == '\n' || *cursor == '\r')
    {
        ++cursor;
    }
    const char* rollback = text;
    const char* finish;
    i32 state = 0;
    u8 character;
    const char* next;
    i32 parsed_exponent;
    while (true)
    {
        character = static_cast<u8>(*cursor);
        next = cursor + 1;
        finish = rollback;
        switch (state)
        {
        case 0:
            if (static_cast<signed char>(character) > '0' && static_cast<signed char>(character) < ':')
            {
                state = 3;
                continue;
            }
            if (character == static_cast<u8>(g_crt_decimal_point))
            {
                state = 5;
                cursor = next;
                continue;
            }
            if (character == '+')
            {
                sign = 0;
                state = 2;
                cursor = next;
            }
            else if (character == '-')
            {
                sign = 0x8000;
                state = 2;
                cursor = next;
            }
            else
            {
                state = 1;
                cursor = next;
                if (character != '0')
                {
                    goto finish_before;
                }
            }
            continue;
        case 1:
            saw_number = 1;
            if (static_cast<signed char>(character) > '0' && static_cast<signed char>(character) < ':')
            {
                state = 3;
                continue;
            }
            state = 4;
            cursor = next;
            if (character != static_cast<u8>(g_crt_decimal_point))
            {
                if (character == '+' || character == '-')
                {
                    goto implicit_sign;
                }
                state = 1;
                if (character != '0')
                {
                    goto exponent_letter;
                }
            }
            continue;
        case 2:
            if (static_cast<signed char>(character) > '0' && static_cast<signed char>(character) < ':')
            {
                state = 3;
                continue;
            }
            if (character == static_cast<u8>(g_crt_decimal_point))
            {
                state = 5;
                cursor = next;
            }
            else
            {
                state = 1;
                cursor = next;
                if (character != '0')
                {
                    goto finish_parse;
                }
            }
            continue;
        case 3:
            saw_number = 1;
            while (decimal_digit_type(character) != 0)
            {
                if (count < 25)
                {
                    digits[count++] = character - '0';
                }
                else
                {
                    ++adjustment;
                }
                character = static_cast<u8>(*next++);
            }
            state = 4;
            cursor = next;
            if (character != static_cast<u8>(g_crt_decimal_point))
            {
                goto exponent_or_finish;
            }
            continue;
        case 4:
            saw_number = 1;
            saw_decimal = 1;
            if (count == 0)
            {
                while (character == '0')
                {
                    --adjustment;
                    character = static_cast<u8>(*next++);
                }
            }
            while (decimal_digit_type(character) != 0)
            {
                if (count < 25)
                {
                    digits[count++] = character - '0';
                    --adjustment;
                }
                character = static_cast<u8>(*next++);
            }
            goto exponent_or_finish;
        case 5:
            saw_decimal = 1;
            state = 4;
            if (decimal_digit_type(character) != 0)
            {
                continue;
            }
            goto finish_parse;
        case 6:
            rollback = cursor - 1;
            if (static_cast<signed char>(character) < '1' || static_cast<signed char>(character) > '9')
            {
                if (character == '+')
                {
                    goto positive_exponent;
                }
                if (character == '-')
                {
                    goto negative_exponent;
                }
                finish = rollback;
                if (character != '0')
                {
                    goto finish_parse;
                }
                state = 8;
                cursor = next;
                continue;
            }
            state = 9;
            continue;
        case 7:
            if (static_cast<signed char>(character) < '1' || static_cast<signed char>(character) > '9')
            {
                if (character == '0')
                {
                    state = 8;
                    cursor = next;
                    continue;
                }
                goto finish_parse;
            }
            state = 9;
            continue;
        case 8:
            saw_exponent = 1;
            while (character == '0')
            {
                character = static_cast<u8>(*next++);
            }
            if (static_cast<signed char>(character) < '1' || static_cast<signed char>(character) > '9')
            {
                goto finish_before;
            }
            state = 9;
            cursor = next - 1;
            continue;
        case 9:
            saw_exponent = 1;
            parsed_exponent = 0;
            while (decimal_digit_type(character) != 0)
            {
                parsed_exponent = static_cast<signed char>(character) - '0' + parsed_exponent * 10;
                if (parsed_exponent > 5200)
                {
                    parsed_exponent = 5201;
                    break;
                }
                character = static_cast<u8>(*next++);
            }
            while (true)
            {
                magnitude = parsed_exponent;
                if (decimal_digit_type(character) == 0)
                {
                    break;
                }
                character = static_cast<u8>(*next++);
            }
            goto finish_before;
        case 11:
            if (implicit_exponent != 0)
            {
                rollback = cursor;
                if (character == '+')
                {
                    goto positive_exponent;
                }
                finish = cursor;
                if (character != '-')
                {
                    goto finish_parse;
                }
                goto negative_exponent;
            }
            state = 10;
            next = cursor;
            [[fallthrough]];
        default:
            cursor = next;
            finish = next;
            if (state != 10)
            {
                continue;
            }
            goto finish_parse;
        }
exponent_or_finish:
        if (character == '+' || character == '-')
        {
implicit_sign:
            state = 11;
            cursor = next - 1;
            continue;
        }
exponent_letter:
        if (static_cast<signed char>(character) < 'D' ||
            (static_cast<signed char>(character) > 'E' &&
                (static_cast<signed char>(character) < 'd' || static_cast<signed char>(character) > 'e')))
        {
            goto finish_before;
        }
        state = 6;
        cursor = next;
        continue;
negative_exponent:
        exponent_sign = -1;
positive_exponent:
        state = 7;
        cursor = next;
    }
finish_before:
    finish = next - 1;
finish_parse:
    end = finish;
    if (saw_number == 0)
    {
        status = 4;
    }
    else
    {
        u32 tail = count;
        if (count > 24)
        {
            if (digits[23] > 4)
            {
                ++digits[23];
            }
            count = 24;
            ++adjustment;
            --tail;
        }
        if (count != 0)
        {
            while (digits[--tail] == 0)
            {
                --count;
                ++adjustment;
            }
            crt_decimal_digits_to_extended(digits.data(), count, output);
            i32 exponent = magnitude;
            if (exponent_sign < 0)
            {
                exponent = -magnitude;
            }
            exponent += adjustment;
            if (saw_exponent == 0)
            {
                exponent += scale;
            }
            if (saw_decimal == 0)
            {
                exponent -= decimal_position;
            }
            if (exponent > 5200)
            {
                output = {0, 0, 0x7FFF8000u};
                status = 2;
                goto sign_result;
            }
            if (exponent > -5201)
            {
                crt_scale_extended_decimal(output, exponent, preserve_tail);
                goto sign_result;
            }
            status = 1;
        }
    }
    output = {0, 0, 0};
sign_result:
    output[2] |= u32(sign) << 16;
    return status;
}

// Xbox 0x0016AD9A
CrtDecimalResult* crt_parse_decimal_result(CrtDecimalResult& output, const char* text)
{
    CrtMantissa extended;
    const char* end;
    u32 flags = 0;
    const u32 parsed = crt_parse_extended(extended, end, text, 0, 0, 0, 0);
    double value;
    if ((parsed & 4) == 0)
    {
        const u32 converted = crt_extended_to_double({reinterpret_cast<const u8*>(extended.data()), 12}, value);
        if ((parsed & 2) != 0 || converted == 1)
        {
            flags = 0x80;
        }
        if ((parsed & 1) != 0 || converted == 2)
        {
            flags |= 0x100;
        }
    }
    else
    {
        flags = 0x200;
        value = 0;
    }
    output.flags = flags;
    output.consumed = static_cast<i32>(end - text);
    output.value = value;
    return &output;
}

// Xbox 0x00166B9D
double crt_ascii_to_double(const char* text)
{
    while (true)
    {
        u32 whitespace;
        if (g_crt_mb_cur_max < 2)
        {
            whitespace = static_cast<u8>(g_crt_pctype[static_cast<u8>(*text)]) & 8;
        }
        else
        {
            whitespace = crt_character_type(static_cast<u8>(*text), 8);
        }
        if (whitespace == 0)
        {
            break;
        }
        ++text;
    }
    const char* cursor = text;
    do
    {
        ++cursor;
    }
    while (cursor[-1] != 0);
    CrtDecimalResult output;
    return crt_parse_decimal_result(output, text)->value;
}
}
