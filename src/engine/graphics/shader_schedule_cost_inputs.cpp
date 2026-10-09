#include "shader_schedule_cost_inputs.h"
#include "shader_schedule_hazards.h"
#include "shader_schedule_issue.h"
#include "shader_schedule_record.h"
#include "shader_schedule_init.h"
#include "shader_schedule_cost.h"
#include "shader_merge_optimizer.h"
#include "../audio/listener_settings.h"
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
float load_float(const u8* state, u32 offset)
{
    float value;
    std::memcpy(&value, state + offset, sizeof(value));
    return value;
}
void store_float(u8* state, u32 offset, float value)
{
    std::memcpy(state + offset, &value, sizeof(value));
}
}

// Xbox 0x0018B9C9
long double runtime_shader_first_operand_cost(u8* state, const u32* instruction, u32 mask, float issued, u32& reason)
{
    const u32 word = instruction[2];
    return runtime_shader_operand_hazard_cost(state, instruction, (instruction[1] >> 28) & 3, word & 15, (word >> 12) & 1,
        (word >> 10) & 3, (word >> 8) & 3, (word >> 6) & 3, (word >> 4) & 3, 0, mask, issued, reason);
}

// Xbox 0x0018BA2D
long double runtime_shader_second_operand_cost(u8* state, const u32* instruction, u32 mask, float issued, u32& reason)
{
    const u32 word = instruction[1];
    return runtime_shader_operand_hazard_cost(state, instruction, (word >> 13) & 3, (word >> 15) & 15, (word >> 27) & 1,
        (word >> 25) & 3, (word >> 23) & 3, (word >> 21) & 3, (word >> 19) & 3, 1, mask, issued, reason);
}

// Xbox 0x0018BA95
long double runtime_shader_third_operand_cost(u8* state, const u32* instruction, u32 mask, float issued, u32& reason)
{
    const u32 word = instruction[1];
    return runtime_shader_operand_hazard_cost(state, instruction, (instruction[0] >> 28) & 3, word & 15, (word >> 12) & 1,
        (word >> 10) & 3, (word >> 8) & 3, (word >> 6) & 3, (word >> 4) & 3, 2, mask, issued, reason);
}

// Xbox 0x0018BE49
bool runtime_shader_first_operand_ready(u8* state, const u32* instruction, u32 mask, i32 issued)
{
    const u32 word = instruction[2];
    return runtime_shader_operand_hazard_ready(state, instruction, (instruction[1] >> 28) & 3, word & 15, (word >> 12) & 1,
        (word >> 10) & 3, (word >> 8) & 3, (word >> 6) & 3, (word >> 4) & 3, 0, mask, issued);
}

// Xbox 0x0018BEA8
bool runtime_shader_second_operand_ready(u8* state, const u32* instruction, u32 mask, i32 issued)
{
    const u32 word = instruction[1];
    return runtime_shader_operand_hazard_ready(state, instruction, (word >> 13) & 3, (word >> 15) & 15, (word >> 27) & 1,
        (word >> 25) & 3, (word >> 23) & 3, (word >> 21) & 3, (word >> 19) & 3, 1, mask, issued);
}

// Xbox 0x0018BF0B
bool runtime_shader_third_operand_ready(u8* state, const u32* instruction, u32 mask, i32 issued)
{
    const u32 word = instruction[1];
    return runtime_shader_operand_hazard_ready(state, instruction, (instruction[0] >> 28) & 3, word & 15, (word >> 12) & 1,
        (word >> 10) & 3, (word >> 8) & 3, (word >> 6) & 3, (word >> 4) & 3, 2, mask, issued);
}

// Xbox 0x0018BF95
long double runtime_shader_instruction_cost(u8* state, const u32* instruction, u32& reason)
{
    reason = 0x20;
    u8 masks[3];
    if (state[0x10] == 0)
    {
        for (u32 operand = 0; operand < 3; ++operand)
        {
            masks[operand] = runtime_shader_operand_channel_mask(instruction, operand);
        }
        u32 delay = 0;
        do
        {
            const i32 cycle = std::bit_cast<i32>(static_cast<u32>(truncate_runtime_float(
                static_cast<long double>(delay) + load_float(state, 0x14))));
            if (runtime_shader_first_operand_ready(state, instruction, masks[0], cycle)
                && runtime_shader_second_operand_ready(state, instruction, masks[1], cycle)
                && runtime_shader_third_operand_ready(state, instruction, masks[2], cycle))
            {
                break;
            }
            ++delay;
        } while (delay < 5);
        return static_cast<long double>(delay);
    }
    for (u32 operand = 0; operand < 3; ++operand)
    {
        masks[operand] = runtime_shader_operand_channel_mask(instruction, operand);
    }
    u32 candidate_reason = 0x20;
    long double candidate = runtime_shader_first_operand_cost(state, instruction, masks[0],
        load_float(state, 0x14), candidate_reason);
    const float first = static_cast<float>(candidate);
    float maximum = first;
    reason = candidate_reason;
    candidate = runtime_shader_second_operand_cost(state, instruction, masks[1],
        load_float(state, 0x14), candidate_reason);
    if (static_cast<long double>(first) < candidate || reason == 0x20)
    {
        maximum = static_cast<float>(candidate);
        reason = candidate_reason;
    }
    candidate = runtime_shader_third_operand_cost(state, instruction, masks[2],
        load_float(state, 0x14), candidate_reason);
    if (static_cast<long double>(maximum) < candidate || reason == 0x20)
    {
        maximum = static_cast<float>(candidate);
        reason = candidate_reason;
    }
    return maximum;
}

// Xbox 0x0018C0DB
u32 issue_runtime_shader_instruction(u8* state, const u32* instruction, float* cost, u32* reason)
{
    *reason = 0x20;
    if (state[0x10] == 0)
    {
        const u32 delay = static_cast<u32>(truncate_runtime_float(runtime_shader_instruction_cost(state, instruction, *reason)));
        long double value = std::bit_cast<i32>(delay);
        if (std::bit_cast<i32>(delay) < 0)
        {
            value += 4294967296.0L; // DAT_0032B144.
        }
        float output_cost = static_cast<float>(value);
        store_float(state, 0x14, static_cast<float>(value + load_float(state, 0x14)));
        const u32 result = record_runtime_shader_register_writes(state, instruction);
        if (state[0x10] == 0)
        {
            store_float(state, 0x14, static_cast<float>(static_cast<long double>(load_float(state, 0x14)) + 1.0L));
        }
        else
        {
            store_float(state, 0x14, static_cast<float>(static_cast<long double>(load_float(state, 0x14)) + 0.5L));
            output_cost = static_cast<float>(delay / 3);
        }
        *cost = output_cost;
        return result;
    }
    const long double value = runtime_shader_instruction_cost(state, instruction, *reason);
    const float output_cost = static_cast<float>(value);
    store_float(state, 0x14, static_cast<float>(value + load_float(state, 0x14)));
    const double threshold = std::bit_cast<double>(u64{0x3FC851EB851EB852}); // DAT_00203760.
    if (static_cast<long double>(threshold) < value)
    {
        advance_runtime_shader_issue_schedule(state + 0x1C0);
    }
    const u32 result = record_runtime_shader_issue_writes(state, instruction, std::bit_cast<i32>(*reason));
    store_float(state, 0x14, static_cast<float>(static_cast<long double>(load_float(state, 0x14)) + 0.5L));
    *cost = output_cost;
    return result;
}

// Xbox 0x0018C277
u32 estimate_runtime_shader_program_cost(u8* state, u8 mode, const u32* program, float* cost)
{
    float total = 0;
    const i32 count = std::bit_cast<i32>(program[0]);
    u32 result = configure_runtime_shader_schedule(state, mode, program[0]);
    if (std::bit_cast<i32>(result) >= 0)
    {
        i32 index = 0;
        float instruction_cost = 0;
        u32 reason = 0x20;
        if (count > 0)
        {
            const u32* instruction = program + 1;
            do
            {
                if (std::bit_cast<i32>(result) < 0)
                {
                    break;
                }
                result = issue_runtime_shader_instruction(state, instruction, &instruction_cost, &reason);
                if (mode == 1)
                {
                    total = static_cast<float>(static_cast<long double>(instruction_cost) + total + 0.5L);
                }
                else
                {
                    total = static_cast<float>(static_cast<long double>(instruction_cost) + total + 1.0L);
                }
                ++index;
                instruction += 3;
            } while (index < count);
        }
        ShaderScheduleCostState tail;
        std::memcpy(tail.pending.data(), state, tail.pending.size());
        tail.enabled = state[0x10];
        tail.issued = load_float(state, 0x14);
        const long double final_cost = runtime_shader_schedule_tail_cost(tail, reason) + total;
        if (std::bit_cast<i32>(result) >= 0)
        {
            *cost = static_cast<float>(final_cost);
        }
    }
    return result;
}

// Xbox 0x0018C1B9
u32 optimize_runtime_shader_multiply_order(u8* state, u8 mode, u32* program)
{
    const i32 count = std::bit_cast<i32>(program[0]);
    u32 result = configure_runtime_shader_schedule(state, mode, program[0]);
    if (std::bit_cast<i32>(result) >= 0 && count > 0)
    {
        i32 index = 0;
        u32* instruction = program + 1;
        do
        {
            if (std::bit_cast<i32>(result) < 0)
            {
                return result;
            }
            u32 swapped[3];
            if (swap_runtime_shader_multiply_operands(swapped, instruction))
            {
                u32 reason = 0x20;
                const float original_cost = static_cast<float>(runtime_shader_instruction_cost(state, instruction, reason));
                const long double swapped_cost = runtime_shader_instruction_cost(state, swapped, reason);
                if (swapped_cost < static_cast<long double>(original_cost))
                {
                    instruction[0] = swapped[0];
                    instruction[1] = swapped[1];
                    instruction[2] = swapped[2];
                }
            }
            float cost;
            u32 reason = 0x20;
            result = issue_runtime_shader_instruction(state, instruction, &cost, &reason);
            ++index;
            instruction += 3;
        } while (index < std::bit_cast<i32>(program[0]));
    }
    return result;
}
}
