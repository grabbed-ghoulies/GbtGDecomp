#include "shader_merge_pass.h"
#include "shader_merge_optimizer.h"
#include "shader_pair_optimizer.h"
#include "shader_merge_patterns.h"

namespace gbtg
{
// Xbox 0x00188348
bool retry_runtime_shader_instruction_merge(u32* output, const u32* first, const u32* second,
    u32* error, RuntimeShaderMergeScratch& scratch)
{
    if (merge_runtime_shader_instruction(output, first, second, error))
    {
        return true;
    }
    // Retail fallbacks pass &param_4, overwriting the local argument slot,
    // rather than the error word addressed by the original pointer.
    u32 discarded_error;
    if (convert_runtime_shader_move_to_scalar(scratch.conversion_words, first))
    {
        if (merge_runtime_shader_instruction(output, scratch.conversion_words, second, &discarded_error))
        {
            return true;
        }
    }
    if (convert_runtime_shader_move_to_scalar(scratch.conversion_words, second))
    {
        if (merge_runtime_shader_instruction(output, first, scratch.conversion_words, &discarded_error))
        {
            return true;
        }
    }
    return false;
}

// Xbox 0x0018A86A
bool combine_runtime_shader_dependent_pair(u32* output, const u32* first, const u32* second,
    RuntimeShaderMergeScratch& scratch)
{
    if (runtime_shader_instruction_dependency(first, second))
    {
        return false;
    }
    u32 discarded_error;
    if (retry_runtime_shader_instruction_merge(output, first, second, &discarded_error, scratch))
    {
        return true;
    }
    u32 swapped[3];
    if (swap_runtime_shader_multiply_operands(swapped, first))
    {
        first = swapped;
    }
    else
    {
        if (!swap_runtime_shader_multiply_operands(swapped, second))
        {
            return false;
        }
        second = swapped;
    }
    if (retry_runtime_shader_instruction_merge(output, first, second, &discarded_error, scratch))
    {
        return true;
    }
    return false;
}

// Xbox 0x0018ADFB
u32 compact_runtime_shader_merge_runs(u32* program, RuntimeShaderMergeScratch& scratch)
{
    i32 read = 0;
    i32 written = 0;
    if (static_cast<i32>(program[0]) > 0)
    {
        u32* destination = program + 1;
        do
        {
            const u32* source = program + read * 3 + 1;
            u32 current[3] = {source[0], source[1], source[2]};
            if (read < static_cast<i32>(program[0]) - 1)
            {
                source += 3;
                do
                {
                    u32 combined[3];
                    if (!combine_runtime_shader_dependent_pair(combined, current, source, scratch))
                    {
                        break;
                    }
                    ++read;
                    source += 3;
                    current[0] = combined[0];
                    current[1] = combined[1];
                    current[2] = combined[2];
                } while (read < static_cast<i32>(program[0]) - 1);
            }
            destination[0] = current[0];
            destination[1] = current[1];
            ++read;
            ++written;
            destination[2] = current[2];
            destination += 3;
        } while (read < static_cast<i32>(program[0]));
    }
    program[0] = static_cast<u32>(written);
    return 0;
}

// Xbox 0x0018ADAF
bool combine_runtime_shader_all_patterns(u32* output, const u32* first, const u32* second,
    RuntimeShaderMergeScratch& scratch)
{
    if (!combine_runtime_shader_dependent_pair(output, first, second, scratch)
        && !combine_runtime_shader_instruction_pair(output, first, second)
        && !combine_runtime_shader_multiply_add(output, first, second)
        && !combine_runtime_shader_matching_operations(output, first, second))
    {
        return false;
    }
    return true;
}
// Xbox 0x0018AE8A
u32 compact_runtime_shader_all_merge_runs(u32* program, RuntimeShaderMergeScratch& scratch)
{
    i32 read = 0;
    i32 written = 0;
    if (static_cast<i32>(program[0]) > 0)
    {
        u32* destination = program + 1;
        do
        {
            const u32* source = program + read * 3 + 1;
            u32 current[3] = {source[0], source[1], source[2]};
            if (read < static_cast<i32>(program[0]) - 1)
            {
                source += 3;
                do
                {
                    u32 combined[3];
                    if (!combine_runtime_shader_all_patterns(combined, current, source, scratch))
                    {
                        break;
                    }
                    ++read;
                    source += 3;
                    current[0] = combined[0];
                    current[1] = combined[1];
                    current[2] = combined[2];
                } while (read < static_cast<i32>(program[0]) - 1);
            }
            destination[0] = current[0];
            destination[1] = current[1];
            ++read;
            ++written;
            destination[2] = current[2];
            destination += 3;
        } while (read < static_cast<i32>(program[0]));
    }
    program[0] = static_cast<u32>(written);
    return 0;
}

}
