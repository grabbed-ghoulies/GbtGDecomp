#include "shader_validation_reserved_register.h"
#include <bit>

namespace gbtg
{
// Xbox 0x0018EFC4
void resolve_shader_validation_reserved_register(u32* program)
{
    i32 count = std::bit_cast<i32>(program[0]);
    if (count > 0)
    {
        u32 next = 2;
        u32 offset = 5;
        do
        {
            if ((program[offset - 4] & 0xF00000u) == 0xF00000u)
            {
                // Four STOSD instructions clear all sixteen native slots.
                u8 used[16] = {};
                if (std::bit_cast<i32>(next - 1) < count)
                {
                    used[(program[offset - 1] >> 20) & 15u] = 1;
                    used[program[offset + 1] & 15u] = 1;
                    const u32 operand = program[offset];
                    used[(operand >> 15) & 15u] = 1;
                    used[operand & 15u] = 1;
                }
                if (std::bit_cast<i32>(next) < count)
                {
                    used[(program[offset + 2] >> 20) & 15u] = 1;
                    used[program[offset + 4] & 15u] = 1;
                    const u32 operand = program[offset + 3];
                    used[(operand >> 15) & 15u] = 1;
                    used[operand & 15u] = 1;
                }
                i32 reg = 11;
                if (used[11] != 0)
                {
                    while (reg > 0)
                    {
                        --reg;
                        if (used[reg] == 0)
                        {
                            break;
                        }
                    }
                }
                const u32 value = program[offset - 4];
                program[offset - 4] = ((static_cast<u32>(reg) << 20) ^ value) & 0xF00000u ^ value;
            }
            ++next;
            count = std::bit_cast<i32>(program[0]);
            offset += 3;
        } while (std::bit_cast<i32>(next - 2) < count);
    }
}
}
