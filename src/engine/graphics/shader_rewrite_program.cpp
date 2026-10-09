#include "shader_rewrite_program.h"
#include "shader_schedule_cost_inputs.h"
#include <bit>

namespace gbtg
{
// Xbox 0x0018689E
u32 move_runtime_shader_instruction(u32*& program, i32 destination, i32 source)
{
    i32 offset = source * 3;
    const u32* input = program + offset + 1;
    const u32 saved[3] = {input[0], input[1], input[2]};
    if (source < destination)
    {
        i32 remaining = destination - source;
        do
        {
            u32* current = program + offset;
            --remaining;
            current[1] = current[4];
            current[2] = current[5];
            current[3] = current[6];
            offset += 3;
        } while (remaining != 0);
    }
    else if (destination < source)
    {
        i32 remaining = source - destination;
        do
        {
            u32* current = program + offset;
            current[1] = current[-2];
            current[2] = current[-1];
            offset -= 3;
            --remaining;
            current[3] = current[0];
        } while (remaining != 0);
    }
    u32* output = program + 1 + destination * 3;
    output[0] = saved[0];
    output[1] = saved[1];
    output[2] = saved[2];
    return 0;
}

// Xbox 0x00186913
u32 replace_runtime_shader_instruction_pair(u32*& program, i32 destination, i32 removed, const u32* replacement)
{
    u32* output = program + 1 + destination * 3;
    output[0] = replacement[0];
    output[1] = replacement[1];
    output[2] = replacement[2];
    u32* current_program = program;
    ++removed;
    if (removed < std::bit_cast<i32>(current_program[0]))
    {
        i32 offset = removed * 3;
        do
        {
            u32* current = current_program + offset;
            current[-2] = current[1];
            current[-1] = current[2];
            current[0] = current[3];
            current_program = program;
            ++removed;
            offset += 3;
        } while (removed < std::bit_cast<i32>(current_program[0]));
    }
    --program[0];
    return 0;
}

// Xbox 0x001869EC
u32 runtime_shader_needs_register_one_rewrite(const u32* program, u8* required)
{
    i32 index = 0;
    u8 found = 0;
    if (std::bit_cast<i32>(program[0]) > 0)
    {
        const u32* instruction = program + 1;
        do
        {
            const u32 opcode = instruction[2];
            const u32 vector_opcode = (opcode >> 25) & 15;
            if ((vector_opcode == 0 && (opcode & 0xE0000000) != 0
                && (instruction[0] & 0xF0000) != 0 && (instruction[0] & 0xF00000) != 0x100000)
                || (vector_opcode != 0 && (opcode & 0xE0000000) == 0
                    && (instruction[0] & 0x0F000000) != 0 && (instruction[0] & 0xF00000) == 0x100000))
            {
                found = 1;
                break;
            }
            ++index;
            instruction += 3;
        } while (index < std::bit_cast<i32>(program[0]));
    }
    *required = found;
    return 0;
}

// Xbox 0x0018C329
bool runtime_shader_instruction_has_cost(u8* schedule, const u32* instruction)
{
    u32 reason = 0x20;
    return 0.0L < runtime_shader_instruction_cost(schedule, instruction, reason);
}
}
