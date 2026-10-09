#include "shader_merge_optimizer.h"
#include "shader_pair_optimizer.h"
#include "shader_swizzle.h"

namespace gbtg
{
namespace
{
// Retail tables at 0x001A22A0, 0x001A22B0 and 0x001A22C0.
constexpr u8 first_operand[16] = {0,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0};
constexpr u8 second_operand[16] = {0,0,1,0,1,1,1,1,1,1,1,1,1,0,0,0};
constexpr u8 third_operand[16] = {0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0};
}

// Xbox 0x00187B22
bool combine_runtime_shader_multiply_add(u32* output, const u32* first, const u32* second)
{
    if ((first[2] & 0x1E000000u) != 0x04000000u || (first[2] & 0xE0000000u) != 0)
    {
        return false;
    }
    const u32 second_opcode = second[2];
    if ((second_opcode & 0x1E000000u) != 0x06000000u || (second_opcode & 0xE0000000u) != 0)
    {
        return false;
    }
    const u32 first_word = first[0];
    const u32 first_mask = (first_word >> 24) & 15u;
    if (first_mask == 0 || (first_word & 0xF000u) != 0)
    {
        return false;
    }
    const u32 second_word = second[0];
    const u32 second_mask = (second_word >> 24) & 15u;
    if (second_mask != first_mask)
    {
        return false;
    }
    const u32 destination = (first_word >> 20) & 15u;
    if (((second_word >> 20) & 15u) != destination)
    {
        return false;
    }
    if (((second_word >> 12) & ~(second_word >> 24) & 15u) != 0)
    {
        return false;
    }
    bool third_is_destination = true;
    if ((second[1] & 0x30000000u) != 0x10000000u || (second_opcode & 15u) != destination)
    {
        third_is_destination = false;
    }
    bool first_is_destination = true;
    if ((second_word & 0x30000000u) != 0x10000000u || (second[1] & 15u) != destination)
    {
        first_is_destination = false;
    }
    if (first_is_destination == third_is_destination)
    {
        return false;
    }
    if (third_is_destination)
    {
        const u32 operand = second[2];
        if (!runtime_shader_swizzle_is_identity(static_cast<u8>(second_mask),
            static_cast<i32>((operand >> 12) & 1u), static_cast<i32>((operand >> 10) & 3u),
            static_cast<i32>((operand >> 8) & 3u), static_cast<i32>((operand >> 6) & 3u),
            static_cast<i32>((operand >> 4) & 3u)))
        {
            return false;
        }
    }
    if (first_is_destination)
    {
        const u32 operand = second[1];
        if (!runtime_shader_swizzle_is_identity(static_cast<u8>(second_mask),
            static_cast<i32>((operand >> 12) & 1u), static_cast<i32>((operand >> 10) & 3u),
            static_cast<i32>((operand >> 8) & 3u), static_cast<i32>((operand >> 6) & 3u),
            static_cast<i32>((operand >> 4) & 3u)))
        {
            return false;
        }
    }
    const bool second_vertex = runtime_shader_uses_vertex_input(second);
    const bool first_constant = runtime_shader_uses_constant(first);
    const bool second_constant = runtime_shader_uses_constant(second);
    const bool first_vertex = runtime_shader_uses_vertex_input(first);
    if (first_vertex && second_vertex && ((second_opcode ^ first[2]) & 0x1E000u) != 0)
    {
        return false;
    }
    if (first_constant && second_constant && ((second_opcode ^ first[2]) & 0x1FE0000u) != 0)
    {
        return false;
    }
    output[0] = first[0];
    output[1] = first[1];
    output[2] = first[2];
    const u32 merged_opcode = (output[2] & 0xE9FFFFFFu) | 0x08000000u;
    output[2] = merged_opcode;
    if (second_vertex)
    {
        output[2] = ((second[2] ^ merged_opcode) & 0x1E000u) ^ merged_opcode;
    }
    if (second_constant)
    {
        output[2] ^= (output[2] ^ second[2]) & 0x1FE0000u;
    }
    if (third_is_destination)
    {
        output[0] ^= (output[0] ^ second[0]) & 0x30000000u;
        output[1] ^= (second[1] ^ output[1]) & 0x1000u;
        output[1] ^= (second[1] ^ output[1]) & 15u;
        output[1] ^= (second[1] ^ output[1]) & 0xC00u;
        output[1] ^= (second[1] ^ output[1]) & 0x300u;
        output[1] ^= (second[1] ^ output[1]) & 0xC0u;
        output[1] ^= (second[1] ^ output[1]) & 0x30u;
    }
    if (first_is_destination)
    {
        output[0] ^= (second[1] ^ output[0]) & 0x30000000u;
        output[1] ^= (output[1] ^ second[2]) & 0x1000u;
        output[1] ^= (second[2] ^ output[1]) & 15u;
        output[1] ^= (second[2] ^ output[1]) & 0xC00u;
        output[1] ^= (second[2] ^ output[1]) & 0x300u;
        output[1] ^= (second[2] ^ output[1]) & 0xC0u;
        output[1] ^= (second[2] ^ output[1]) & 0x30u;
    }
    output[0] ^= (output[0] ^ second[0]) & 4u;
    output[0] ^= (second[0] ^ output[0]) & 0xF000u;
    output[0] ^= (second[0] ^ output[0]) & 0xFF8u;
    return true;
}

// Xbox 0x00187057
bool convert_runtime_shader_move_to_scalar(u32* output, const u32* input)
{
    if ((input[2] & 0x1E000000u) == 0x02000000u && (input[2] & 0xE0000000u) == 0)
    {
        u32 value = (output[2] & 0x01FFFFFFu) | 0x20000000u;
        output[2] = value;
        value ^= (input[2] ^ value) & 0x01FE0000u;
        output[2] = value;
        output[2] = (input[2] & 0x1E000u) | (value & 0xFFFE01B0u) | 0x1B0u;
        value = (output[1] & 0xD0D83FFFu) | 0x10D82000u;
        output[1] = value;
        value ^= (input[2] ^ value) & 0x1000u;
        output[1] = value;
        value ^= (input[2] ^ value) & 0xC00u;
        output[1] = value;
        value ^= (input[2] ^ value) & 0x300u;
        output[1] = value;
        value ^= (input[2] ^ value) & 0xC0u;
        output[1] = value;
        output[1] = ((input[2] ^ value) & 0x30u) ^ value;
        output[0] ^= (input[1] ^ output[0]) & 0x30000000u;
        output[1] ^= (output[1] ^ input[2]) & 15u;
        value = (output[0] & 0xF00FFFFFu) | (input[0] & 0xF00000u);
        output[0] = value;
        value ^= (input[0] ^ value) & 0xFF8u;
        output[0] = value;
        value ^= ((static_cast<u32>((input[0] & 0xF000u) != 0) << 2) ^ value) & 4u;
        output[0] = value;
        value ^= (input[0] ^ value) & 1u;
        output[0] = value;
        value ^= (input[0] ^ value) & 2u;
        output[0] = value;
        value ^= ((input[0] >> 8) ^ value) & 0xF0000u;
        output[0] = value;
        output[0] = ((input[0] ^ value) & 0xF000u) ^ value;
        return true;
    }
    return false;
}

// Xbox 0x001871AD
bool swap_runtime_shader_multiply_operands(u32* output, const u32* input)
{
    if ((input[2] & 0x1E000000u) == 0x06000000u && (input[2] & 0xE0000000u) == 0)
    {
        output[0] = input[0];
        output[1] = input[1];
        output[2] = input[2];
        output[1] ^= (output[1] ^ input[0]) & 0x30000000u;
        output[2] ^= (output[2] ^ input[1]) & 0x1000u;
        output[2] ^= (input[1] ^ output[2]) & 0xFu;
        output[2] ^= (input[1] ^ output[2]) & 0xC00u;
        output[2] ^= (input[1] ^ output[2]) & 0x300u;
        output[2] ^= (input[1] ^ output[2]) & 0xC0u;
        output[2] ^= (input[1] ^ output[2]) & 0x30u;
        output[0] ^= (input[1] ^ output[0]) & 0x30000000u;
        output[1] ^= (output[1] ^ input[2]) & 0x1000u;
        output[1] ^= (input[2] ^ output[1]) & 0xFu;
        output[1] ^= (input[2] ^ output[1]) & 0xC00u;
        output[1] ^= (input[2] ^ output[1]) & 0x300u;
        output[1] ^= (input[2] ^ output[1]) & 0xC0u;
        output[1] ^= (input[2] ^ output[1]) & 0x30u;
        return true;
    }
    return false;
}

// Xbox 0x00187522
bool equal_runtime_shader_operand(u32 type, u32 index, u32 negate, const u32* first,
    u32 other_type, u32 other_index, u32 other_negate, const u32* second)
{
    if (type == other_type && negate == other_negate)
    {
        if (type != 1 || index != other_index)
        {
            if ((type != 2 || ((first[2] ^ second[2]) & 0x1E000u) != 0)
                && (type != 3 || ((first[2] ^ second[2]) & 0x1FE0000u) != 0
                    || ((first[0] ^ second[0]) & 2u) != 0))
            {
                return false;
            }
        }
        return true;
    }
    return false;
}

// Xbox 0x00187E46
u8 runtime_shader_uses_first_operand(const u32* instruction)
{
    return first_operand[(instruction[2] >> 25) & 15u];
}

// Xbox 0x00187E5C
u8 runtime_shader_uses_second_operand(const u32* instruction)
{
    return second_operand[(instruction[2] >> 25) & 15u];
}

// Xbox 0x00187E72
bool runtime_shader_uses_third_operand(const u32* instruction)
{
    if (third_operand[(instruction[2] >> 25) & 15u] == 0
        && (instruction[2] & 0xE0000000u) == 0)
    {
        return false;
    }
    return true;
}

// Xbox 0x00187A59
bool runtime_shader_vector_uses_type(const u32* instruction, u32 type)
{
    const u32 opcode = (instruction[2] >> 25) & 15u;
    if ((first_operand[opcode] == 0 || ((instruction[1] >> 28) & 3u) != type)
        && (second_operand[opcode] == 0 || ((instruction[1] >> 13) & 3u) != type)
        && (third_operand[opcode] == 0 || ((instruction[0] >> 28) & 3u) != type))
    {
        return false;
    }
    return true;
}

// Xbox 0x00187AB8
bool runtime_shader_scalar_uses_type(const u32* instruction, u32 type)
{
    if ((instruction[2] & 0xE0000000u) == 0 || ((instruction[0] >> 28) & 3u) != type)
    {
        return false;
    }
    return true;
}

// Xbox 0x00187ADA
bool runtime_shader_uses_type(const u32* instruction, u32 type)
{
    if (!runtime_shader_vector_uses_type(instruction, type)
        && !runtime_shader_scalar_uses_type(instruction, type))
    {
        return false;
    }
    return true;
}

// Xbox 0x00187B06
bool runtime_shader_uses_constant(const u32* instruction)
{
    return runtime_shader_uses_type(instruction, 3);
}

// Xbox 0x00187B14
bool runtime_shader_uses_vertex_input(const u32* instruction)
{
    return runtime_shader_uses_type(instruction, 2);
}

// Xbox 0x00187F19
bool pack_runtime_shader_destinations(u32* instruction, const u8* destinations)
{
    if (destinations[2] == 0 || destinations[5] == 0
        || (destinations[1] != 1 && destinations[4] == 1))
    {
        instruction[0] ^= ((static_cast<u32>(destinations[0]) << 24) ^ instruction[0]) & 0xF000000u;
        instruction[0] ^= ((static_cast<u32>(destinations[3]) << 16) ^ instruction[0]) & 0xF0000u;
        u8 destination;
        if (destinations[2] == 0)
        {
            destination = destinations[4];
        }
        else
        {
            destination = destinations[1];
        }
        instruction[0] ^= ((static_cast<u32>(destination) << 20) ^ instruction[0]) & 0xF00000u;
        return true;
    }
    return false;
}

// Xbox 0x00187F88
bool merge_runtime_shader_destinations(u32* output, const u32* first, const u32* second)
{
    u8 first_destinations[6];
    u8 second_destinations[6];
    u8 merged[6];
    decode_runtime_shader_write_registers(first_destinations, first);
    decode_runtime_shader_write_registers(second_destinations, second);
    i32 index = 0;
    do
    {
        merged[index] = first_destinations[index];
        merged[index + 1] = first_destinations[index + 1];
        merged[index + 2] = first_destinations[index + 2];
        if (first_destinations[index + 2] != 0 && second_destinations[index + 2] != 0)
        {
            if (first_destinations[index + 1] != second_destinations[index + 1]
                || first_destinations[index] != second_destinations[index])
            {
                return false;
            }
        }
        if (second_destinations[index + 2] != 0)
        {
            merged[index] = second_destinations[index];
            merged[index + 1] = second_destinations[index + 1];
            merged[index + 2] = second_destinations[index + 2];
        }
        index += 3;
    } while (index < 6);
    const u32 opcode = output[2];
    if ((opcode & 0x1E000000u) == 0 || (opcode & 0xE0000000u) == 0
        || ((merged[5] == 0 || merged[4] == 1) && (merged[2] == 0 || merged[1] != 1)))
    {
        return pack_runtime_shader_destinations(output, merged);
    }
    return false;
}

// Xbox 0x00188032
bool merge_runtime_shader_instruction(u32* output, const u32* first, const u32* second, u32* error)
{
    output[0] = first[0];
    output[1] = first[1];
    output[2] = first[2];
    const u32 second_vector_opcode = (second[2] >> 25) & 15u;
    if (second_vector_opcode != 0)
    {
        const u32 first_vector_opcode = (first[2] >> 25) & 15u;
        if (first_vector_opcode != 0 && first_vector_opcode != second_vector_opcode)
        {
            *error = 0x7B;
            return false;
        }
        output[2] ^= (output[2] ^ second[2]) & 0x1E000000u;
    }
    const u32 second_scalar_bits = second[2];
    if ((second_scalar_bits >> 29) != 0)
    {
        if ((first[2] >> 29) != 0 && (first[2] >> 29) != (second_scalar_bits >> 29))
        {
            *error = 0x7B;
            return false;
        }
        output[2] = ((output[2] ^ second_scalar_bits) & 0x1FFFFFFFu) ^ second_scalar_bits;
    }
    if (runtime_shader_uses_first_operand(second) != 0)
    {
        if (runtime_shader_uses_first_operand(first) != 0)
        {
            const u32 type = (second[1] >> 28) & 3u;
            if (type != ((first[1] >> 28) & 3u)
                || ((first[2] ^ second[2]) & 0x1000u) != 0
                || (type == 1 && ((first[2] ^ second[2]) & 15u) != 0))
            {
                *error = 0x7C;
                return false;
            }
        }
        output[1] ^= (second[1] ^ output[1]) & 0x30000000u;
        output[2] ^= (second[2] ^ output[2]) & 0x1000u;
        output[2] ^= (second[2] ^ output[2]) & 15u;
    }
    if (runtime_shader_uses_second_operand(second) != 0)
    {
        if (runtime_shader_uses_second_operand(first) != 0)
        {
            const u32 type = (second[1] >> 13) & 3u;
            if (type != ((first[1] >> 13) & 3u)
                || ((second[1] ^ first[1]) & 0x8000000u) != 0
                || (type == 1 && ((second[1] ^ first[1]) & 0x78000u) != 0))
            {
                *error = 0x7C;
                return false;
            }
        }
        output[1] ^= (second[1] ^ output[1]) & 0x6000u;
        output[1] ^= (second[1] ^ output[1]) & 0x8000000u;
        output[1] ^= (second[1] ^ output[1]) & 0x78000u;
    }
    if (runtime_shader_uses_third_operand(second))
    {
        if (runtime_shader_uses_third_operand(first))
        {
            const u32 type = (second[0] >> 28) & 3u;
            if (type != ((first[0] >> 28) & 3u)
                || ((second[1] ^ first[1]) & 0x1000u) != 0
                || (type == 1 && ((second[1] ^ first[1]) & 15u) != 0))
            {
                *error = 0x7C;
                return false;
            }
        }
        output[0] ^= (output[0] ^ second[0]) & 0x30000000u;
        output[1] ^= (second[1] ^ output[1]) & 0x1000u;
        output[1] ^= (second[1] ^ output[1]) & 15u;
    }
    if (!merge_runtime_shader_swizzles(output, first, second))
    {
        *error = 0x7D;
        return false;
    }
    if (runtime_shader_uses_constant(second))
    {
        if (runtime_shader_uses_constant(first)
            && (((first[2] ^ second[2]) & 0x1FE0000u) != 0 || ((first[0] ^ second[0]) & 2u) != 0))
        {
            *error = 0x7C;
            return false;
        }
        output[2] ^= (second[2] ^ output[2]) & 0x1FE0000u;
        output[0] ^= (output[0] ^ second[0]) & 2u;
    }
    if (runtime_shader_uses_vertex_input(second))
    {
        if (runtime_shader_uses_vertex_input(first) && ((first[2] ^ second[2]) & 0x1E000u) != 0)
        {
            *error = 0x7C;
            return false;
        }
        output[2] ^= (second[2] ^ output[2]) & 0x1E000u;
    }
    u32 second_output = second[0];
    const u32 second_mask = (second_output >> 12) & 15u;
    if (second_mask != 0)
    {
        const u32 first_output = first[0];
        const u32 first_mask = (first_output >> 12) & 15u;
        if (first_mask != 0
            && (((second_output ^ first_output) & 4u) != 0 || second_mask != first_mask
                || ((second[0] ^ first_output) & 0xFF8u) != 0))
        {
            *error = 0x7E;
            return false;
        }
        if (first_mask != 0)
        {
            second_output = second[0];
        }
        output[0] ^= (output[0] ^ second_output) & 0xF000u;
        output[0] ^= (second[0] ^ output[0]) & 4u;
        output[0] ^= (second[0] ^ output[0]) & 0xFF8u;
    }
    if (merge_runtime_shader_destinations(output, first, second))
    {
        return true;
    }
    *error = 0x7E;
    return false;
}
}
