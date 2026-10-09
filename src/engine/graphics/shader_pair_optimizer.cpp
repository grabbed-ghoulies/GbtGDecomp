#include "shader_pair_optimizer.h"

#include <bit>

namespace gbtg
{
namespace
{
// Retail byte tables at 0x001A22A0, 0x001A22B0 and 0x001A22C0.
constexpr u8 uses_first_input[16] = {0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0};
constexpr u8 uses_second_input[16] = {0, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0};
constexpr u8 uses_third_input[16] = {0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
}

// Xbox 0x00187E9B
void decode_runtime_shader_write_registers(u8* output, const u32* instruction)
{
    output[0] = static_cast<u8>((instruction[0] >> 24) & 0xF);
    output[1] = static_cast<u8>((instruction[0] >> 20) & 0xF);
    if ((instruction[0] & 0xF000000) == 0 || (instruction[2] & 0x1E000000) == 0)
    {
        output[2] = 0;
    }
    else
    {
        output[2] = 1;
    }
    output[3] = static_cast<u8>((instruction[0] >> 16) & 0xF);
    if ((instruction[2] & 0xE0000000) == 0)
    {
        output[4] = 7;
    }
    else if ((instruction[2] & 0x1E000000) == 0)
    {
        output[4] = static_cast<u8>((instruction[0] >> 20) & 0xF);
    }
    else
    {
        output[4] = 1;
    }
    if ((instruction[0] & 0xF0000) == 0 || (instruction[2] & 0xE0000000) == 0)
    {
        output[5] = 0;
    }
    else
    {
        output[5] = 1;
    }
}

// Xbox 0x0018735A
bool runtime_shader_has_constant_input(const u32* instruction)
{
    const u32 opcode = (instruction[2] >> 25) & 0xF;
    if (((uses_first_input[opcode] == 0 || (instruction[1] & 0x30000000) != 0x30000000) &&
         (uses_second_input[opcode] == 0 || (instruction[1] & 0x6000) != 0x6000) &&
         (uses_third_input[opcode] == 0 || (instruction[0] & 0x30000000) != 0x30000000)) &&
        ((instruction[2] & 0xE0000000) == 0 || (instruction[0] & 0x30000000) != 0x30000000))
    {
        return false;
    }
    return true;
}

// Xbox 0x001873C8
bool runtime_shader_constant_inputs_compatible(const u32* first, const u32* second)
{
    if ((((first[2] ^ second[2]) & 0x1FE0000) != 0 || ((first[0] ^ second[0]) & 2) != 0) &&
        runtime_shader_has_constant_input(first) && runtime_shader_has_constant_input(second))
    {
        return false;
    }
    return true;
}

// Xbox 0x0018731B
bool runtime_shader_swizzle_is_identity(u8 mask, i32 negate, i32 x, i32 y, i32 z, i32 w)
{
    if (negate == 0 && ((mask & 1) == 0 || w == 3) && ((mask & 2) == 0 || z == 2) &&
        ((mask & 4) == 0 || y == 1) && ((mask & 8) == 0 || x == 0))
    {
        return true;
    }
    return false;
}

// Xbox 0x0018A506
bool runtime_shader_instruction_dependency(const u32* first, const u32* second)
{
    u8 writes[6];
    decode_runtime_shader_write_registers(writes, first);
    const u32 first_word = first[0];
    const u32 output_mask = first_word >> 12;
    bool writes_output;
    bool writes_temporary_output;
    if ((output_mask & 0xF) == 0 || (first_word & 0x800) != 0)
    {
        writes_output = false;
    }
    else
    {
        writes_output = true;
    }
    if ((output_mask & 0xF) != 0 && (first_word & 0x800) != 0)
    {
        writes_temporary_output = true;
    }
    else
    {
        writes_temporary_output = false;
    }
    const u32 opcode = (second[2] >> 25) & 0xF;
    const bool first_input = uses_first_input[opcode] != 0;
    const bool second_input = uses_second_input[opcode] != 0;
    bool third_input;
    if ((second[2] & 0xE0000000) != 0 || uses_third_input[opcode] != 0)
    {
        third_input = true;
    }
    else
    {
        third_input = false;
    }
    u32 first_mask;
    if (!first_input)
    {
        first_mask = 0;
    }
    else
    {
        const u32 word = second[2];
        first_mask = (1u << (3 - ((word >> 10) & 3))) | (1u << (3 - ((word >> 8) & 3))) |
                     (1u << (3 - ((word >> 6) & 3))) | (1u << (3 - ((word >> 4) & 3)));
    }
    u32 second_mask;
    if (!second_input)
    {
        second_mask = 0;
    }
    else
    {
        const u32 word = second[1];
        second_mask = (1u << (3 - ((word >> 25) & 3))) | (1u << (3 - ((word >> 23) & 3))) |
                      (1u << (3 - ((word >> 21) & 3))) | (1u << (3 - ((word >> 19) & 3)));
    }
    u32 third_mask;
    if (third_input)
    {
        const u32 word = second[1];
        third_mask = (1u << (3 - ((word >> 10) & 3))) | (1u << (3 - ((word >> 8) & 3))) |
                     (1u << (3 - ((word >> 6) & 3))) | (1u << (3 - ((word >> 4) & 3)));
    }
    else
    {
        third_mask = 0;
    }
    const bool constant_input = (first_input && (second[1] & 0x30000000) == 0x30000000) ||
                                (second_input && (second[1] & 0x6000) == 0x6000) ||
                                (third_input && (second[0] & 0x30000000) == 0x30000000);
    i32 index = 0;
    const u8* write = writes;
    do
    {
        if (write[2] != 0 &&
            ((first_input && (second[1] & 0x30000000) == 0x10000000 &&
              (second[2] & 0xF) == write[1] && (write[0] & first_mask) != 0) ||
             (second_input && (second[1] & 0x6000) == 0x2000 &&
              ((second[1] >> 15) & 0xF) == write[1] && (write[0] & second_mask) != 0) ||
             (third_input && (second[0] & 0x30000000) == 0x10000000 &&
              (second[1] & 0xF) == write[1] && (write[0] & third_mask) != 0)))
        {
            return true;
        }
        ++index;
        write += 3;
    } while (index < 2);
    if (writes_output && constant_input &&
        (((second[2] >> 14) & 0x7F8) == (first_word & 0xFF8) || (second[0] & 2) != 0))
    {
        return true;
    }
    if (writes_temporary_output && (first_word & 0xFF8) == 0x800 &&
        ((first_input && (second[1] & 0x30000000) == 0x10000000 &&
          (second[2] & 0xF) == 0xC && (output_mask & first_mask & 0xF) != 0) ||
         (second_input && (second[1] & 0x6000) == 0x2000 &&
          (second[1] & 0x78000) == 0x60000 && (output_mask & second_mask & 0xF) != 0) ||
         (third_input && (second[0] & 0x30000000) == 0x10000000 &&
          (second[1] & 0xF) == 0xC && (output_mask & third_mask & 0xF) != 0)))
    {
        return true;
    }
    if ((first[2] & 0x1E000000) == 0x1A000000 && constant_input && (second[0] & 2) != 0)
    {
        return true;
    }
    return false;
}

// Xbox 0x0018A8DD
bool combine_runtime_shader_parallel_instructions(u32* output, const u32* first, const u32* second)
{
    const u32 third_difference = second[2] ^ first[2];
    if ((third_difference & 0x1E000000) == 0 && (third_difference & 0xE0000000) == 0)
    {
        const u32 middle_difference = second[1] ^ first[1];
        const bool first_input_matches = (middle_difference & 0x30000000) == 0 &&
            (third_difference & 0x1000) == 0 && (third_difference & 0xF) == 0 &&
            (third_difference & 0x30) == 0 && (third_difference & 0xC00) == 0 &&
            (third_difference & 0x300) == 0 && (third_difference & 0xC0) == 0;
        const bool second_input_matches = (middle_difference & 0x6000) == 0 &&
            (middle_difference & 0x8000000) == 0 && (middle_difference & 0x78000) == 0 &&
            (middle_difference & 0x180000) == 0 && (middle_difference & 0x6000000) == 0 &&
            (middle_difference & 0x1800000) == 0 && (middle_difference & 0x600000) == 0;
        const bool third_input_matches = ((second[0] ^ first[0]) & 0x30000000) == 0 &&
            (middle_difference & 0x1000) == 0 && (middle_difference & 0xF) == 0 &&
            (middle_difference & 0x30) == 0 && (middle_difference & 0xC00) == 0 &&
            (middle_difference & 0x300) == 0 && (middle_difference & 0xC0) == 0;
        if (first_input_matches && second_input_matches && third_input_matches &&
            (third_difference & 0x1FE0000) == 0 && (third_difference & 0x1E000) == 0 &&
            ((second[0] ^ first[0]) & 2) == 0 && !runtime_shader_instruction_dependency(first, second))
        {
            const bool first_has_no_register_write = (first[0] & 0xF0F0000) == 0;
            const bool second_has_register_write = (second[0] & 0xF0F0000) != 0;
            if ((first_has_no_register_write || !second_has_register_write) &&
                ((first[0] & 0xF000) == 0 || (second[0] & 0xF000) == 0))
            {
                output[0] = first[0];
                output[1] = first[1];
                output[2] = first[2];
                output[0] |= second[0] & 0xF000000;
                output[0] |= second[0] & 0xF0000;
                if (second_has_register_write && first_has_no_register_write)
                {
                    output[0] ^= (second[0] ^ output[0]) & 0xF00000;
                }
                output[0] |= second[0] & 0xF000;
                if ((second[0] & 0xF000) != 0 && (first[0] & 0xF000) == 0)
                {
                    output[0] ^= (output[0] ^ second[0]) & 0xFF8;
                    output[0] ^= (second[0] ^ output[0]) & 4;
                }
                return true;
            }
        }
    }
    return false;
}

// Xbox 0x00187406
bool combine_runtime_shader_move_instruction(u32* output, const u32* first, const u32* second)
{
    const u32 first_word = first[0];
    if ((first_word & 0xF000) == 0 && ((first_word >> 24) & 0xF) != 0 &&
        (second[2] & 0x1E000000) == 0x2000000 && (second[2] & 0xE0000000) == 0 &&
        (second[1] & 0x30000000) == 0x10000000 && (((first_word >> 20) ^ second[2]) & 0xF) == 0)
    {
        const u32 second_word = second[0];
        const u32 mask = (second_word >> 12) & 0xF;
        // 0x0018731B preserves EDX, which holds second_word, rather than returning an EDX:EAX pair.
        if (runtime_shader_swizzle_is_identity(static_cast<u8>(mask), (second[2] >> 12) & 1,
                (second[2] >> 10) & 3, (second[2] >> 8) & 3, (second[2] >> 6) & 3, (second[2] >> 4) & 3) &&
            (second_word & 4) == 0 && ((~(first_word >> 24) & (second_word >> 12) & 0xF) == 0) &&
            mask != 0 && (second_word & 0xF0F0000) == 0 && runtime_shader_constant_inputs_compatible(first, second))
        {
            output[0] = first[0];
            output[1] = first[1];
            output[2] = first[2];
            output[0] ^= (output[0] ^ second[0]) & 0xF000;
            output[0] ^= (second[0] ^ output[0]) & 4;
            output[0] ^= (second[0] ^ output[0]) & 0xFF8;
            return true;
        }
    }
    return false;
}

// Xbox 0x0018ACFB
bool combine_runtime_shader_instruction_pair(u32* output, const u32* first, const u32* second)
{
    if (!combine_runtime_shader_parallel_instructions(output, first, second) &&
        !combine_runtime_shader_move_instruction(output, first, second))
    {
        return false;
    }
    return true;
}

// Xbox 0x0018AD2D
u32 compact_runtime_shader_instruction_pairs(u32* program)
{
    i32 output_count = 0;
    i32 index = 0;
    if (0 < std::bit_cast<i32>(program[0]))
    {
        u32* output = program + 1;
        const u32* next = program + 4;
        const u32* current = program + 1;
        do
        {
            u32 combined[3];
            const u32* selected = current;
            if (index < std::bit_cast<i32>(program[0]) - 1 &&
                combine_runtime_shader_instruction_pair(combined, current, next))
            {
                ++index;
                next += 3;
                current += 3;
                selected = combined;
            }
            ++index;
            next += 3;
            current += 3;
            ++output_count;
            u32* destination = output;
            output += 3;
            destination[0] = selected[0];
            destination[1] = selected[1];
            destination[2] = selected[2];
        } while (index < std::bit_cast<i32>(program[0]));
    }
    program[0] = static_cast<u32>(output_count);
    return 0;
}
}
