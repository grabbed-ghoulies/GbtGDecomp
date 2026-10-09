#include "shader_rewrite_writes.h"
#include <cstring>

namespace gbtg
{
// Xbox 0x001866C9
void initialize_runtime_shader_writes(u8* writes)
{
    std::memset(writes, 0, 0xDF);
}

// Xbox 0x00186724
u32 accumulate_runtime_shader_writes(u8* writes, const u32* instruction)
{
    u32 result = 0;
    if ((instruction[2] & 0x1E000000u) == 0x1A000000u)
    {
        writes[0xDD] = 1;
    }
    u32 word = instruction[0];
    if ((word & 0x0F000000u) != 0)
    {
        const u32 destination = (word >> 20) & 15u;
        if (destination < 12)
        {
            writes[destination] |= static_cast<u8>((word >> 24) & 15u);
        }
        else
        {
            result = 0x80004005u;
        }
    }
    word = instruction[0];
    if ((word & 0x000F0000u) != 0)
    {
        u32 destination;
        if ((instruction[2] & 0x1E000000u) == 0 || (instruction[2] & 0xE0000000u) == 0)
        {
            destination = (word >> 20) & 15u;
        }
        else
        {
            destination = 1;
        }
        writes[destination] |= static_cast<u8>((word >> 16) & 15u);
    }
    word = instruction[0];
    const u8 mask = static_cast<u8>((word >> 12) & 15u);
    if (mask != 0)
    {
        const u32 destination = (word >> 3) & 255u;
        if (((word >> 11) & 1u) == 0)
        {
            if (destination < 0xC0)
            {
                writes[destination + 0xD] |= mask;
                writes[0xDE] |= static_cast<u8>((instruction[0] >> 12) & 15u);
                return result;
            }
        }
        else if (destination < 0x10)
        {
            writes[destination + 0xCD] = mask;
            if (destination != 0)
            {
                return result;
            }
            writes[0xC] |= static_cast<u8>((instruction[0] >> 12) & 15u);
            return result;
        }
        result = 0x80004005u;
    }
    return result;
}

// Xbox 0x001866F8
bool runtime_shader_write_masks_overlap(const u8* first, const u8* second, u32 count)
{
    u32 index = 0;
    if (count != 0)
    {
        do
        {
            if ((first[index] & second[index]) != 0)
            {
                return true;
            }
            ++index;
        } while (index < count);
    }
    return false;
}

// Xbox 0x00189C00
bool runtime_shader_writes_conflict(const u8* first, const u8* second)
{
    if (!runtime_shader_write_masks_overlap(first, second, 0xD)
        && !runtime_shader_write_masks_overlap(first + 0xD, second + 0xD, 0xC0)
        && !runtime_shader_write_masks_overlap(first + 0xCD, second + 0xCD, 0x10))
    {
        if (first[0xDD] == 0)
        {
            return false;
        }
        if (second[0xDD] == 0)
        {
            return false;
        }
    }
    return true;
}

// Xbox 0x00186806
bool runtime_shader_operand_reads_writes(const u8* writes, const u32* instruction, u32 type, u32 reg,
    u32 negate, u32 x, u32 y, u32 z, u32 w, u8 mask)
{
    // The retail helper accepts but does not test operand negation.
    (void)negate;
    u8 channels = 0;
    bool conflict = false;
    if ((mask & 8) != 0)
    {
        channels = static_cast<u8>(1u << ((3u - x) & 31u));
    }
    if ((mask & 4) != 0)
    {
        channels |= static_cast<u8>(1u << ((3u - y) & 31u));
    }
    if ((mask & 2) != 0)
    {
        channels |= static_cast<u8>(1u << ((3u - z) & 31u));
    }
    if ((mask & 1) != 0)
    {
        channels |= static_cast<u8>(1u << ((3u - w) & 31u));
    }
    u8 written;
    if (type == 1)
    {
        written = writes[reg];
    }
    else
    {
        if (type == 2)
        {
            return false;
        }
        if (type != 3)
        {
            return false;
        }
        conflict = (writes[((instruction[2] >> 17) & 255u) + 0xD] & channels) != 0;
        if ((instruction[0] & 2) == 0)
        {
            return conflict;
        }
        written = writes[0xDE];
    }
    if ((written & channels) != 0)
    {
        conflict = true;
    }
    return conflict;
}

// Xbox 0x00189C62
bool runtime_shader_first_operand_reads_writes(const u8* writes, const u32* instruction, u8 mask)
{
    const u32 word = instruction[2];
    return runtime_shader_operand_reads_writes(writes, instruction, (instruction[1] >> 28) & 3u,
        word & 15u, (word >> 12) & 1u, (word >> 10) & 3u, (word >> 8) & 3u,
        (word >> 6) & 3u, (word >> 4) & 3u, mask);
}

// Xbox 0x00189CB3
bool runtime_shader_second_operand_reads_writes(const u8* writes, const u32* instruction, u8 mask)
{
    const u32 word = instruction[1];
    return runtime_shader_operand_reads_writes(writes, instruction, (word >> 13) & 3u,
        (word >> 15) & 15u, (word >> 27) & 1u, (word >> 25) & 3u, (word >> 23) & 3u,
        (word >> 21) & 3u, (word >> 19) & 3u, mask);
}

// Xbox 0x00189D06
bool runtime_shader_third_operand_reads_writes(const u8* writes, const u32* instruction, u8 mask)
{
    const u32 word = instruction[1];
    return runtime_shader_operand_reads_writes(writes, instruction, (instruction[0] >> 28) & 3u,
        word & 15u, (word >> 12) & 1u, (word >> 10) & 3u, (word >> 8) & 3u,
        (word >> 6) & 3u, (word >> 4) & 3u, mask);
}

// Xbox 0x00189D56
u32 runtime_shader_vector_reads_writes(const u8* writes, const u32* instruction, u8* conflict)
{
    const u8 mask = static_cast<u8>(((instruction[0] >> 24) | (instruction[0] >> 12)) & 15u);
    switch ((instruction[2] >> 25) & 15u)
    {
    case 0:
        return 0;
    case 1:
        *conflict = runtime_shader_first_operand_reads_writes(writes, instruction, mask);
        return 0;
    case 2:
    case 9:
    case 10:
    case 11:
    case 12:
        *conflict = runtime_shader_first_operand_reads_writes(writes, instruction, mask)
            || runtime_shader_second_operand_reads_writes(writes, instruction, mask);
        return 0;
    case 3:
        *conflict = runtime_shader_first_operand_reads_writes(writes, instruction, mask);
        *conflict = *conflict != 0 || runtime_shader_third_operand_reads_writes(writes, instruction, mask);
        return 0;
    case 4:
        *conflict = runtime_shader_first_operand_reads_writes(writes, instruction, mask);
        *conflict = *conflict != 0 || runtime_shader_second_operand_reads_writes(writes, instruction, mask);
        *conflict = *conflict != 0 || runtime_shader_third_operand_reads_writes(writes, instruction, mask);
        return 0;
    case 5:
        *conflict = runtime_shader_first_operand_reads_writes(writes, instruction, 14)
            || runtime_shader_second_operand_reads_writes(writes, instruction, 14);
        return 0;
    case 6:
        *conflict = runtime_shader_first_operand_reads_writes(writes, instruction, 14)
            || runtime_shader_second_operand_reads_writes(writes, instruction, 15);
        return 0;
    case 7:
        *conflict = runtime_shader_first_operand_reads_writes(writes, instruction, 15)
            || runtime_shader_second_operand_reads_writes(writes, instruction, 15);
        return 0;
    case 8:
        *conflict = runtime_shader_first_operand_reads_writes(writes, instruction, 6)
            || runtime_shader_second_operand_reads_writes(writes, instruction, 5);
        return 0;
    case 13:
        *conflict = runtime_shader_first_operand_reads_writes(writes, instruction, 8);
        return 0;
    default:
        return 0x80004005u;
    }
}

// Xbox 0x00189EAD
u32 runtime_shader_scalar_reads_writes(const u8* writes, const u32* instruction, u8* conflict)
{
    u8 mask = static_cast<u8>(((instruction[0] >> 16) | (instruction[0] >> 12)) & 15u);
    const u32 opcode = instruction[2] >> 29;
    if (opcode == 0)
    {
        return 0;
    }
    if (opcode != 1)
    {
        if (opcode < 2)
        {
            return 0x80004005u;
        }
        if (opcode < 7)
        {
            mask = 8;
        }
        else
        {
            if (opcode != 7)
            {
                return 0x80004005u;
            }
            mask = 13;
        }
    }
    *conflict = runtime_shader_third_operand_reads_writes(writes, instruction, mask);
    return 0;
}

// Xbox 0x0018B384
bool runtime_shader_instruction_reads_writes(const u8* writes, const u32* instruction)
{
    const u32 opcodes = instruction[2];
    u8 vector_conflict = 0;
    u8 scalar_conflict = 0;
    u32 result = 0;
    if ((opcodes & 0x1E000000u) != 0)
    {
        result = runtime_shader_vector_reads_writes(writes, instruction, &vector_conflict);
    }
    if (static_cast<i32>(result) >= 0 && (opcodes & 0xE0000000u) != 0)
    {
        result = runtime_shader_scalar_reads_writes(writes, instruction, &scalar_conflict);
    }
    const bool relative_conflict = writes[0xDD] != 0 && (instruction[0] & 2) != 0;
    return static_cast<i32>(result) >= 0 && (vector_conflict != 0 || scalar_conflict != 0 || relative_conflict);
}
}
