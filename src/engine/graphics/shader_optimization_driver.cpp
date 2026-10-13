#include "shader_optimization_driver.h"
#include "shader_pair_optimizer.h"
#include "shader_rewrite_context.h"
#include "shader_reorder.h"
#include "shader_schedule_cost_inputs.h"
#include "vertex_shader_code.h"
#include <bit>
#include <cstdlib>
#include <cstring>

namespace gbtg
{
// Xbox 0x0018C563
u32 optimize_runtime_vertex_shader(u8 pairs, u32 passes, u32, u32 mode,
    u32* program, u32 scheduling, u8 validation, RuntimeShaderOptimizationContext& context)
{
    if (pairs != 0 && u8(scheduling) == 0 && validation == 0)
    {
        scheduling &= 0xFFFFFF00;
        validation = 1;
    }
    if (u8(mode) != 1 || (mode & 0x200) != 0)
    {
        if (u8(scheduling) != 0 || validation != 0)
        {
            scheduling = (scheduling & 0xFFFFFF00) | 1;
        }
        else
        {
            scheduling &= 0xFFFFFF00;
        }
        validation = 0;
    }
    u32 result = 0;
    u32* alternate = nullptr;
    bool invalid = false;
    i32 previous_count;
    float first_cost;
    float second_cost;
    if (validation != 0)
    {
        alternate = static_cast<u32*>(std::malloc(0x1324)); // XMemAlloc 0x000F7270.
        if (alternate == nullptr)
        {
            result = 0x8007000E;
        }
        alternate[0] = program[0];
        i32 index = 0;
        if (std::bit_cast<i32>(program[0]) > 0)
        {
            do
            {
                alternate[index * 3 + 1] = program[index * 3 + 1];
                alternate[index * 3 + 2] = program[index * 3 + 2];
                alternate[index * 3 + 3] = program[index * 3 + 3];
                ++index;
            } while (index < std::bit_cast<i32>(program[0]));
        }
        if (pairs != 0 || u8(passes) != 0)
        {
            if (!validate_shader_validation_program(context.validation, alternate))
            {
                invalid = true;
                if (u8(scheduling) == 0)
                {
                    result = 0xFFFFEFE5;
                }
            }
        }
    }
    if (u8(scheduling) != 0)
    {
        do
        {
            previous_count = std::bit_cast<i32>(program[0]);
            if (std::bit_cast<i32>(result) < 0)
            {
                goto cleanup;
            }
            if (pairs != 0)
            {
                result = compact_runtime_shader_instruction_pairs(program);
            }
            if (std::bit_cast<i32>(result) < 0)
            {
                goto cleanup;
            }
            if (u8(passes) != 0)
            {
                result = remove_runtime_shader_dead_writes(context.dead_write_frame, mode, program);
                if (std::bit_cast<i32>(result) < 0)
                {
                    goto cleanup;
                }
                result = 0;
            }
            if (std::bit_cast<i32>(result) < 0)
            {
                goto cleanup;
            }
            if (u8(passes) != 0 && (mode & 0x200) == 0)
            {
                result = rewrite_runtime_shader_register_one(context.registers, mode, program);
                if (std::bit_cast<i32>(result) < 0)
                {
                    goto cleanup;
                }
                result = 0;
            }
            if (std::bit_cast<i32>(result) < 0)
            {
                goto cleanup;
            }
            if (u8(passes) != 0)
            {
                result = reorder_runtime_shader_program(program, context.reorder_frame, u8(mode), context.merge);
                if (std::bit_cast<i32>(result) < 0)
                {
                    goto cleanup;
                }
                result = 0;
            }
            if (std::bit_cast<i32>(result) < 0)
            {
                goto cleanup;
            }
            if (u8(passes) != 0)
            {
                result = optimize_runtime_shader_multiply_order(context.multiply_frame, u8(mode), program);
            }
            if (std::bit_cast<i32>(result) < 0)
            {
                goto cleanup;
            }
            if (pairs != 0)
            {
                result = compact_runtime_shader_merge_runs(program, context.merge);
            }
            if (std::bit_cast<i32>(result) < 0)
            {
                goto cleanup;
            }
            if (u8(passes) != 0)
            {
                result = compact_runtime_shader_all_merge_runs(program, context.merge);
            }
            if (std::bit_cast<i32>(result) < 0)
            {
                goto cleanup;
            }
        } while (u8(passes) != 0 && std::bit_cast<i32>(program[0]) < previous_count);
    }
    if (std::bit_cast<i32>(result) >= 0)
    {
        if (validation == 0)
        {
            return result;
        }
        if (!invalid)
        {
            if (u8(scheduling) != 0)
            {
                first_cost = std::bit_cast<float>(passes);
                second_cost = std::bit_cast<float>(scheduling);
                estimate_runtime_shader_program_cost(context.first_cost_frame, u8(mode), program, &first_cost);
                estimate_runtime_shader_program_cost(context.second_cost_frame, u8(mode), alternate, &second_cost);
                // FCOMP alternate,current; unordered, equal and greater retain current.
                if (!(second_cost < first_cost))
                {
                    goto cleanup;
                }
            }
            program[0] = alternate[0];
            i32 index = 0;
            if (std::bit_cast<i32>(program[0]) > 0)
            {
                do
                {
                    program[index * 3 + 1] = alternate[index * 3 + 1];
                    program[index * 3 + 2] = alternate[index * 3 + 2];
                    program[index * 3 + 3] = alternate[index * 3 + 3];
                    ++index;
                } while (index < std::bit_cast<i32>(program[0]));
            }
        }
    }
cleanup:
    if (validation != 0 && alternate != nullptr)
    {
        std::free(alternate); // XMemFree 0x000F7280.
    }
    return result;
}

// Xbox 0x0018D2F4
u32 splice_runtime_vertex_shaders(u32* output, u32* size, u32* instruction_count,
    const u32* const* programs, u32 count, i32 optimize, RuntimeShaderOptimizationContext* context)
{
    u32 total = 0;
    u32 end = 1;
    if (size != nullptr)
    {
        u32 required = 4;
        u32 index = 0;
        if (count != 0)
        {
            do
            {
                required += (programs[index][0] >> 16) * 0x10;
                ++index;
            } while (index < count);
        }
        if (*size < required)
        {
            *size = required;
            return 1;
        }
    }
    const u16 signature = u16(programs[0][0]);
    u32 index = 0;
    if (count != 0)
    {
        do
        {
            const u32 instructions = programs[index][0] >> 16;
            const u32* source = programs[index] + 1;
            u32* destination = output + end;
            u32 words = instructions << 2;
            while (words != 0)
            {
                *destination = *source;
                ++destination;
                ++source;
                --words;
            }
            total += instructions;
            end += instructions * 4;
            output[end - 1] &= 0xFFFFFFFE;
            ++index;
        } while (index < count);
    }
    if (optimize != 0)
    {
        u32 index = 0;
        if (total != 0)
        {
            do
            {
                u32 unpacked[3];
                unpack_runtime_vertex_instruction(output + index * 4 + 1, unpacked);
                std::memcpy(output + index * 3 + 1, unpacked, sizeof(unpacked));
                ++index;
            } while (index < total);
        }
        output[0] = total;
        optimize_runtime_vertex_shader(1, 1, 0, runtime_vertex_shader_code_type(&signature),
            output, 0, 1, *context);
        total = output[0];
        end = total * 4 + 1;
        u32 packed_index = total - 1;
        if (packed_index != 0xFFFFFFFF)
        {
            u32 remaining = total;
            do
            {
                const u32 first = output[packed_index * 3 + 1];
                const u32 third = output[packed_index * 3 + 3];
                const u32 second = output[packed_index * 3 + 2];
                output[packed_index * 4 + 1] = 0;
                output[packed_index * 4 + 2] = third >> 4;
                output[packed_index * 4 + 3] = ((second >> 2) & 0x0FFFFFFF) | (third << 28);
                output[packed_index * 4 + 4] = (first & 0x3FFFFFFF) | (second << 30);
                --packed_index;
                --remaining;
            } while (remaining != 0);
        }
    }
    output[0] = (total << 16) | signature;
    output[end - 1] |= 1;
    if (instruction_count != nullptr)
    {
        *instruction_count = total;
    }
    return 0;
}
}
