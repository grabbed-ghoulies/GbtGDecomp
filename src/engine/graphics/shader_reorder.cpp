#include "shader_reorder.h"
#include "shader_rewrite_writes.h"
#include "shader_rewrite_program.h"
#include "shader_schedule_init.h"
#include "shader_schedule_cost_inputs.h"
#include <array>
#include <bit>
namespace gbtg
{
// Xbox 0x0018B403
bool runtime_shader_reorder_is_safe(u32*& program, const u8* writes, i32 first, i32 candidate)
{
    std::array<u8, runtime_shader_write_storage_size> candidate_writes;
    initialize_runtime_shader_writes(candidate_writes.data());
    accumulate_runtime_shader_writes(candidate_writes.data(), program + 1 + candidate * 3);
    if (!runtime_shader_writes_conflict(writes, candidate_writes.data()))
    {
        if (first < candidate)
        {
            const u32* instruction = program + 1 + first * 3;
            do
            {
                if (runtime_shader_instruction_reads_writes(candidate_writes.data(), instruction))
                {
                    return false;
                }
                ++first;
                instruction += 3;
            } while (first < candidate);
        }
        return true;
    }
    return false;
}
// Xbox 0x0018B47B
bool find_runtime_shader_merge_candidate(u32*& program, i32 first, i32* candidate, u32* output,
    RuntimeShaderMergeScratch& scratch)
{
    std::array<u8, runtime_shader_write_storage_size> writes;
    initialize_runtime_shader_writes(writes.data());
    accumulate_runtime_shader_writes(writes.data(), program + 1 + first * 3);
    u32* current_program = program;
    i32 index = first + 1;
    if (index < std::bit_cast<i32>(current_program[0]))
    {
        i32 offset = index * 3;
        do
        {
            if (!runtime_shader_instruction_reads_writes(writes.data(), current_program + 1 + offset)
                && combine_runtime_shader_dependent_pair(output, current_program + 1 + first * 3,
                    current_program + 1 + offset, scratch)
                && runtime_shader_reorder_is_safe(program, writes.data(), first, index))
            {
                *candidate = index;
                return true;
            }
            accumulate_runtime_shader_writes(writes.data(), program + 1 + offset);
            current_program = program;
            offset += 3;
            ++index;
        } while (index < std::bit_cast<i32>(current_program[0]));
    }
    return false;
}
// Xbox 0x0018C358
bool find_runtime_shader_ready_candidate(u32*& program, u8* schedule, i32 first, i32* candidate)
{
    std::array<u8, runtime_shader_write_storage_size> writes;
    initialize_runtime_shader_writes(writes.data());
    accumulate_runtime_shader_writes(writes.data(), program + 1 + first * 3);
    u32* current_program = program;
    i32 index = first + 1;
    if (index < std::bit_cast<i32>(current_program[0]))
    {
        i32 offset = index * 3;
        do
        {
            const u32* instruction = current_program + 1 + offset;
            if (!runtime_shader_instruction_reads_writes(writes.data(), instruction)
                && !runtime_shader_instruction_has_cost(schedule, instruction)
                && runtime_shader_reorder_is_safe(program, writes.data(), first, index))
            {
                *candidate = index;
                return true;
            }
            accumulate_runtime_shader_writes(writes.data(), program + 1 + offset);
            current_program = program;
            ++index;
            offset += 3;
        } while (index < std::bit_cast<i32>(current_program[0]));
    }
    return false;
}
// Xbox 0x0018C3F6
u32 reorder_runtime_shader_program(u32*& program, u8* schedule, u8 mode, RuntimeShaderMergeScratch& scratch)
{
    u32 result = configure_runtime_shader_schedule(schedule, mode, program[0]);
    i32 index = 0;
    bool retry = false;
    std::array<u32, 3> merged;
    if (std::bit_cast<i32>(result) >= 0)
    {
        if (std::bit_cast<i32>(program[0] - 1) > 0)
        {
            i32 offset = 0;
            do
            {
                if (std::bit_cast<i32>(result) < 0)
                {
                    return result;
                }
                const u32* instruction = program + 1 + offset;
                i32 candidate;
                if (runtime_shader_instruction_has_cost(schedule, instruction)
                    && find_runtime_shader_ready_candidate(program, schedule, index, &candidate))
                {
                    result = move_runtime_shader_instruction(program, index, candidate);
                }
                if (std::bit_cast<i32>(result) >= 0)
                {
                    i32 removed;
                    if (find_runtime_shader_merge_candidate(program, index, &removed, merged.data(), scratch))
                    {
                        if (!runtime_shader_instruction_has_cost(schedule, merged.data())
                            || !find_runtime_shader_ready_candidate(program, schedule, index, &candidate))
                        {
                            result = replace_runtime_shader_instruction_pair(program, index, removed, merged.data());
                        }
                        else
                        {
                            result = move_runtime_shader_instruction(program, index, candidate);
                            retry = true;
                        }
                    }
                    if (std::bit_cast<i32>(result) >= 0)
                    {
                        u32 reason = 0x20;
                        float cost;
                        result = issue_runtime_shader_instruction(schedule, instruction, &cost, &reason);
                    }
                }
                offset += 3;
                ++index;
            } while (index < std::bit_cast<i32>(program[0] - 1));
        }
        if (std::bit_cast<i32>(result) >= 0 && retry)
        {
            result = configure_runtime_shader_schedule(schedule, mode, program[0]);
            if (std::bit_cast<i32>(result) >= 0)
            {
                index = 0;
                if (std::bit_cast<i32>(program[0] - 1) > 0)
                {
                    do
                    {
                        if (std::bit_cast<i32>(result) < 0)
                        {
                            return result;
                        }
                        i32 removed;
                        if (find_runtime_shader_merge_candidate(program, index, &removed, merged.data(), scratch))
                        {
                            result = replace_runtime_shader_instruction_pair(program, index, removed, merged.data());
                        }
                        ++index;
                    } while (index < std::bit_cast<i32>(program[0] - 1));
                }
            }
        }
    }
    return result;
}
}
