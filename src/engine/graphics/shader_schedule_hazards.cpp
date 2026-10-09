#include "shader_schedule_hazards.h"

#include "shader_swizzle.h"
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
template<typename T>
T read_state(const u8* state, u32 offset)
{
    T value;
    std::memcpy(&value, state + offset, sizeof(value));
    return value;
}
}

// Xbox 0x001872C1
void remove_runtime_shader_vector_instruction(u32* instruction)
{
    const u32 third = instruction[2];
    if ((third & 0x1E000000) != 0)
    {
        u32 first = instruction[0];
        if ((first & 0xF0000) == 0)
        {
            instruction[0] = first & 0xF00FFFFF;
        }
        else
        {
            first = (first & 0xFF1FFFFF) | 0x100000;
            instruction[0] = (((first << 8) ^ first) & 0xF000000) ^ first;
        }
        const u32 second = instruction[1];
        instruction[2] = third & 0xE1FFE000;
        instruction[1] = (second & 0xE0005FFF) | 0x20004000;
    }
}

// Xbox 0x00185F25
long double runtime_shader_register_hazard_cost(const u8* table, u8 reg, u32 channel, u8 scalar, u32& reason)
{
    u32 reason_index;
    u32 cost_index;
    if (scalar == 0)
    {
        reason_index = channel + 0xE0 + static_cast<u32>(reg) * 4;
        cost_index = channel + static_cast<u32>(reg) * 4;
    }
    else
    {
        reason_index = channel + 0x118 + static_cast<u32>(reg) * 4;
        cost_index = channel + 0x38 + static_cast<u32>(reg) * 4;
    }
    reason = read_state<u32>(table, reason_index * 4);
    return read_state<float>(table, cost_index * 4);
}

// Xbox 0x00189723
long double runtime_shader_masked_hazard_cost(const u8* state, const u32* instruction, u8 operand, u32 reg, u8 mask, float /*issued*/, u32& reason)
{
    float cost = 0.0f;
    reason = 0x20;
    i32 channel = 0;
    do
    {
        if ((mask & (1u << channel)) != 0)
        {
            u8 scalar;
            if (operand == 2 && (instruction[2] & 0xE0000000) == 0)
            {
                scalar = 1;
            }
            else
            {
                scalar = 0;
            }
            u32 candidate_reason = 0x20;
            const long double candidate = runtime_shader_register_hazard_cost(state + 0x1C0,
                static_cast<u8>(reg), static_cast<u32>(channel), scalar, candidate_reason);
            // FCOMP / TEST AH,0x41 / JP skips greater and unordered; equal replaces the reason.
            if (static_cast<long double>(cost) <= candidate)
            {
                cost = static_cast<float>(candidate);
                reason = candidate_reason;
            }
        }
        ++channel;
        if (3 < channel)
        {
            return cost;
        }
    } while (true);
}

// Xbox 0x001897A3
bool runtime_shader_register_bypass_ready(const u8* table, const u32* instruction, i32 operand, u8 reg, u8 mask, i32 issued)
{
    bool ready = false;
    i32 channel = 0;
    do
    {
        const u32 bit = 1u << channel;
        if ((bit & mask) != 0)
        {
            const u32 index = static_cast<u32>(channel) + static_cast<u32>(reg) * 4;
            const u32 ready_cycle = read_state<u32>(table, index * 4);
            if (issued != std::bit_cast<i32>(ready_cycle - 3))
            {
                return false;
            }
            const u32 producer = read_state<u32>(table, (index + 0x34) * 4);
            if (producer != 1)
            {
                if (operand != 2)
                {
                    return false;
                }
                const u32 opcode = (instruction[2] >> 25) & 0xF;
                if (opcode != 3 && opcode != 4)
                {
                    return false;
                }
                if ((instruction[2] & 0xE0000000) != 0)
                {
                    u32 vector_removed[3];
                    vector_removed[0] = instruction[0];
                    vector_removed[1] = instruction[1];
                    vector_removed[2] = instruction[2];
                    remove_runtime_shader_vector_instruction(vector_removed);
                    u8 operand_masks[3];
                    decode_runtime_shader_operand_masks(vector_removed, operand_masks);
                    if ((operand_masks[2] & static_cast<u8>(bit)) != 0)
                    {
                        return false;
                    }
                }
                if (producer != 0 && producer != 2)
                {
                    return false;
                }
            }
            ready = true;
        }
        ++channel;
        if (3 < channel)
        {
            return ready;
        }
    } while (true);
}

// Xbox 0x0018B243
bool runtime_shader_masked_hazard_ready(const u8* state, const u32* instruction, u8 operand, u32 reg, u8 mask, i32 issued)
{
    i32 channel = 0;
    while (true)
    {
        const u8 bit = static_cast<u8>(1u << channel);
        const u32 index = static_cast<u32>(channel) + 8 + static_cast<u32>(static_cast<u8>(reg)) * 4;
        if ((mask & bit) != 0 && issued < read_state<i32>(state, index * 4) &&
            !runtime_shader_register_bypass_ready(state + 0x20, instruction, operand, static_cast<u8>(reg), bit, issued))
        {
            break;
        }
        ++channel;
        if (3 < channel)
        {
            return true;
        }
    }
    return false;
}

// Xbox 0x0018B1EE
long double runtime_shader_operand_hazard_cost(u8* state, const u32* instruction, i32 type, u32 reg,
    u32 /*negate*/, u32 /*w*/, u32 /*z*/, u32 /*y*/, u32 /*x*/, u32 operand, u32 mask, float issued, u32& reason)
{
    reason = 0x20;
    if (type == 1)
    {
        return runtime_shader_masked_hazard_cost(state, instruction, static_cast<u8>(operand), reg,
            static_cast<u8>(mask), issued, reason);
    }
    else if (type == 2 || type != 3 || (instruction[0] & 2) == 0)
    {
        return 0.0f; // DAT_0032B100
    }
    reason = read_state<u32>(state, 0x610);
    return read_state<float>(state, 0x290);
}

// Xbox 0x0018BAF8
bool runtime_shader_operand_hazard_ready(u8* state, const u32* instruction, i32 type, u32 reg,
    u32 /*negate*/, u32 /*w*/, u32 /*z*/, u32 /*y*/, u32 /*x*/, u32 operand, u32 mask, i32 issued)
{
    if (type == 1)
    {
        return runtime_shader_masked_hazard_ready(state, instruction, static_cast<u8>(operand), reg,
            static_cast<u8>(mask), issued);
    }
    else
    {
        if (type == 2 || type != 3)
        {
            return true;
        }
        const u32 bank = (instruction[1] >> 2) & 1;
        const u32 previous_cycle = read_state<u32>(state, 0x18 + bank * 4);
        return std::bit_cast<i32>(static_cast<u32>(issued) - previous_cycle) >= 6;
    }
}
}
