#include "shader_rewrite_outputs.h"

namespace gbtg
{
// Xbox 0x00185F5F
bool runtime_shader_vector_register_write_is_identity(const u32* instruction)
{
    const u32 operand = instruction[2];
    if ((operand & 0x1E000000u) == 0x02000000u && (instruction[1] & 0x30000000u) == 0x10000000u)
    {
        const u32 output = instruction[0];
        const u32 mask = output >> 24;
        if ((mask & 15u) != 0 && (((output >> 20) ^ operand) & 15u) == 0
            && (operand & 0x1000u) == 0
            && ((mask & 8u) == 0 || (operand & 0xC00u) == 0)
            && ((mask & 4u) == 0 || (operand & 0x300u) == 0x100u)
            && ((mask & 2u) == 0 || (operand & 0xC0u) == 0x80u))
        {
            if ((output & 0x1000000u) == 0)
            {
                return true;
            }
            if ((operand & 0x30u) == 0x30u)
            {
                return true;
            }
        }
    }
    return false;
}

// Xbox 0x00185FDF
bool runtime_shader_vector_constant_write_is_identity(const u32* instruction)
{
    const u32 operand = instruction[2];
    const u32 output = instruction[0];
    const u32 mask = output >> 12;
    if ((operand & 0x1E000000u) != 0x02000000u || (instruction[1] & 0x30000000u) != 0x30000000u
        || (output & 2u) != 0 || (mask & 15u) == 0 || (output & 4u) != 0
        || ((output >> 3) & 0x100u) != 0
        || static_cast<u8>(operand >> 17) != static_cast<u8>(output >> 3)
        || (operand & 0x1000u) != 0
        || ((mask & 8u) != 0 && (operand & 0xC00u) != 0)
        || ((mask & 4u) != 0 && (operand & 0x300u) != 0x100u)
        || ((mask & 2u) != 0 && (operand & 0xC0u) != 0x80u)
        || ((mask & 1u) != 0 && (operand & 0x30u) != 0x30u))
    {
        return false;
    }
    return true;
}

// Xbox 0x0018607B
bool runtime_shader_scalar_register_write_is_identity(const u32* instruction)
{
    const u32 opcode = instruction[2];
    if ((opcode & 0xE0000000u) == 0x20000000u)
    {
        const u32 output = instruction[0];
        const u32 mask = output >> 16;
        if ((output & 0x30000000u) == 0x10000000u && (mask & 15u) != 0)
        {
            const u32 operand = instruction[1];
            if ((((operand & 15u) == ((output >> 20) & 15u) && (opcode & 0x1E000000u) == 0)
                    || ((opcode & 0x1E000000u) != 0 && (operand & 15u) == 1))
                && (operand & 0x1000u) == 0
                && ((mask & 8u) == 0 || (operand & 0xC00u) == 0)
                && ((mask & 4u) == 0 || (operand & 0x300u) == 0x100u)
                && ((mask & 2u) == 0 || (operand & 0xC0u) == 0x80u)
                && ((mask & 1u) == 0 || (operand & 0x30u) == 0x30u))
            {
                return true;
            }
        }
    }
    return false;
}

// Xbox 0x00186120
bool runtime_shader_scalar_constant_write_is_identity(const u32* instruction)
{
    const u32 output = instruction[0];
    const u32 mask = output >> 12;
    const u32 operand = instruction[1];
    if ((instruction[2] & 0xE0000000u) == 0x20000000u
        && (output & 0x30000000u) == 0x30000000u && (output & 2u) == 0
        && (mask & 15u) != 0 && (output & 4u) != 0
        && ((output >> 3) & 0x100u) == 0
        && static_cast<u8>(instruction[2] >> 17) == static_cast<u8>(output >> 3)
        && (operand & 0x1000u) == 0
        && ((mask & 8u) == 0 || (operand & 0xC00u) == 0)
        && ((mask & 4u) == 0 || (operand & 0x300u) == 0x100u)
        && ((mask & 2u) == 0 || (operand & 0xC0u) == 0x80u)
        && ((mask & 1u) == 0 || (operand & 0x30u) == 0x30u))
    {
        return true;
    }
    return false;
}

// Xbox 0x001861BB
u32 remove_runtime_shader_identity_writes(u32* instruction)
{
    if (runtime_shader_vector_register_write_is_identity(instruction))
    {
        reinterpret_cast<u8*>(instruction)[3] &= 0xF0u;
    }
    if (runtime_shader_vector_constant_write_is_identity(instruction))
    {
        instruction[0] &= 0xFFFF0FFFu;
    }
    if (runtime_shader_scalar_register_write_is_identity(instruction))
    {
        reinterpret_cast<u8*>(instruction)[2] &= 0xF0u;
    }
    if (runtime_shader_scalar_constant_write_is_identity(instruction))
    {
        instruction[0] &= 0xFFFF0FFFu;
    }
    return 0;
}

// Xbox 0x00186201
bool runtime_shader_instruction_outputs_are_redundant(const u32* instruction)
{
    bool vector_redundant = (instruction[2] & 0x1E000000u) == 0;
    bool scalar_redundant = (instruction[2] & 0xE0000000u) == 0;
    if (!vector_redundant)
    {
        const u32 output = instruction[0];
        if (((output & 4u) == 0 && (output & 0xF000u) != 0
                || !runtime_shader_vector_register_write_is_identity(instruction))
            && ((output & 0xF000000u) != 0
                || !runtime_shader_vector_constant_write_is_identity(instruction)))
        {
            return false;
        }
        vector_redundant = true;
    }
    if (!scalar_redundant)
    {
        const u32 output = instruction[0];
        if ((((output & 4u) == 0 || (output & 0xF000u) == 0)
                && runtime_shader_scalar_register_write_is_identity(instruction))
            || ((instruction[0] & 0xF0000u) == 0
                && runtime_shader_scalar_constant_write_is_identity(instruction)))
        {
            scalar_redundant = true;
        }
        else
        {
            scalar_redundant = false;
        }
    }
    if (vector_redundant && scalar_redundant)
    {
        return true;
    }
    return false;
}
}
