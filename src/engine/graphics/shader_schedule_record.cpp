#include "shader_schedule_record.h"
#include "shader_schedule_issue.h"
#include "shader_schedule_update.h"
#include "../audio/listener_settings.h"
#include <cstring>

namespace gbtg
{
namespace
{
// Native scalar latency words at DAT_001A23A0.
constexpr u32 scalar_latency[8] = {4, 2, 2, 2, 2, 2, 2, 2};
float issued_time(const u8* state)
{
    float issued;
    std::memcpy(&issued, state + 0x14, sizeof(issued));
    return issued;
}
u32 issued_cycle(const u8* state)
{
    return static_cast<u32>(truncate_runtime_float(issued_time(state)));
}
}

// Xbox 0x00189499
u32 record_runtime_shader_register_writes(u8* state, const u32* instruction)
{
    u32 result = 0;
    u32 mask = instruction[0] >> 24;
    if ((mask & 15) != 0)
    {
        const u32 reg = instruction[0] >> 20;
        if ((reg & 15) > 11)
        {
            result = 0x80004005;
        }
        const u32 cycle = issued_cycle(state);
        const u32 latency = runtime_shader_vector_latency(instruction);
        write_runtime_shader_register_schedule(state + 0x20, latency,
            static_cast<u8>(reg & 15), static_cast<u8>(mask & 15), cycle);
    }
    mask = instruction[0] >> 16;
    if ((mask & 15) != 0)
    {
        const u32 opcode = instruction[2];
        u8 reg;
        if ((opcode & 0x1E000000) == 0 || (opcode & 0xE0000000) == 0)
        {
            reg = static_cast<u8>((instruction[0] >> 20) & 15);
        }
        else
        {
            reg = 1;
        }
        const u32 cycle = issued_cycle(state);
        write_runtime_shader_register_schedule(state + 0x20, scalar_latency[opcode >> 29],
            reg, static_cast<u8>(mask & 15), cycle);
    }
    const u32 output = instruction[0];
    mask = output >> 12;
    if ((mask & 15) != 0)
    {
        const u32 destination = (output >> 3) & 255;
        if (((output >> 11) & 1) == 0)
        {
            if (destination < 192)
            {
                const u32 value = 1;
                std::memcpy(state + 0x18 + ((destination >> 2) & 1) * 4, &value, sizeof(value));
            }
            else
            {
                result = 0x80004005;
            }
        }
        else if (destination == 0)
        {
            const u32 cycle = issued_cycle(state);
            u32 latency;
            if ((output & 4) == 0)
            {
                latency = runtime_shader_vector_latency(instruction);
            }
            else
            {
                latency = scalar_latency[instruction[2] >> 29];
            }
            write_runtime_shader_register_schedule(state + 0x20, latency, 12,
                static_cast<u8>(mask & 15), cycle);
        }
    }
    return result;
}

// Xbox 0x001895E1
u32 record_runtime_shader_issue_writes(u8* state, const u32* instruction, i32 reason)
{
    u32 result = 0;
    advance_runtime_shader_issue_schedule(state + 0x1C0);
    u32 output = instruction[0];
    if ((output & 0x0F000000) != 0)
    {
        const u8 reg = static_cast<u8>((output >> 20) & 15);
        if (reg > 11)
        {
            result = 0x80004005;
        }
        const float issued = issued_time(state);
        const u8 mask = static_cast<u8>((output >> 24) & 15);
        const u32 latency = runtime_shader_vector_latency(instruction);
        update_runtime_shader_issue_schedule(state + 0x1C0, latency, reg, mask, issued, reason);
    }
    output = instruction[0];
    if ((output & 0xF0000) != 0)
    {
        const u32 opcode = instruction[2];
        u8 reg;
        if ((opcode & 0x1E000000) == 0 || (opcode & 0xE0000000) == 0)
        {
            reg = static_cast<u8>((output >> 20) & 15);
        }
        else
        {
            reg = 1;
        }
        update_runtime_shader_issue_schedule(state + 0x1C0, scalar_latency[opcode >> 29], reg,
            static_cast<u8>((output >> 16) & 15), issued_time(state), reason);
    }
    output = instruction[0];
    if ((output & 0xF000) != 0)
    {
        const u32 destination = (output >> 3) & 255;
        if (((output >> 11) & 1) != 0)
        {
            state[destination] = 1;
            if (destination == 0)
            {
                const float issued = issued_time(state);
                output = instruction[0];
                u32 latency;
                if ((output & 4) == 0)
                {
                    latency = runtime_shader_vector_latency(instruction);
                }
                else
                {
                    latency = scalar_latency[instruction[2] >> 29];
                }
                update_runtime_shader_issue_schedule(state + 0x1C0, latency, 12,
                    static_cast<u8>((output >> 12) & 15), issued, reason);
            }
        }
    }
    if ((instruction[2] & 0x1E000000) == 0x1A000000)
    {
        update_runtime_shader_issue_schedule(state + 0x1C0, 3, 13, 1, issued_time(state), reason);
    }
    return result;
}
}
