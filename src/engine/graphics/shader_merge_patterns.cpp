#include "shader_merge_patterns.h"
#include "shader_merge_optimizer.h"
#include "shader_pair_optimizer.h"
#include "shader_swizzle.h"

namespace gbtg
{
// Xbox 0x0018AB16
bool combine_runtime_shader_matching_operations(u32* output, const u32* first, const u32* second)
{
    const u32 first_opcode = first[2];
    if (((second[2] ^ first_opcode) & 0x1E000000) != 0
        || ((second[2] ^ first_opcode) & 0xE0000000) != 0)
    {
        return false;
    }
    if (!equal_runtime_shader_operand((first[1] >> 28) & 3, first_opcode & 15,
        (first_opcode >> 12) & 1, first, (second[1] >> 28) & 3,
        second[2] & 15, (second[2] >> 12) & 1, second))
    {
        return false;
    }
    const u32 second_middle = second[1];
    const u32 first_middle = first[1];
    if (!equal_runtime_shader_operand((first_middle >> 13) & 3, (first_middle >> 15) & 15,
        (first_middle >> 27) & 1, first, (second_middle >> 13) & 3,
        (second_middle >> 15) & 15, (second_middle >> 27) & 1, second))
    {
        return false;
    }
    const u32 first_output = first[0];
    if (!equal_runtime_shader_operand((first_output >> 28) & 3, first[1] & 15,
        (first[1] >> 12) & 1, first, (second[0] >> 28) & 3,
        second[1] & 15, (second[1] >> 12) & 1, second)
        || runtime_shader_instruction_dependency(first, second))
    {
        return false;
    }
    const bool first_has_no_register_output = (first_output & 0x0F0F0000) == 0;
    const u32 second_output = second[0];
    const bool second_has_register_output = (second_output & 0x0F0F0000) != 0;
    if (!first_has_no_register_output && second_has_register_output
        && ((first_output ^ second_output) & 0xF00000) != 0)
    {
        return false;
    }
    if ((first_output & 0xF000) != 0 && (second_output & 0xF000) != 0
        && ((first_output ^ second_output) & 0xFF8) != 0)
    {
        return false;
    }
    output[0] = first[0];
    output[1] = first[1];
    output[2] = first[2];
    if (!merge_runtime_shader_swizzles(output, first, second))
    {
        return false;
    }
    output[0] |= second[0] & 0x0F000000;
    output[0] |= second[0] & 0xF0000;
    if (second_has_register_output && first_has_no_register_output)
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
