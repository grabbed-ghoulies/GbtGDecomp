#include "shader_schedule_issue.h"
#include <cstring>

namespace gbtg
{
namespace
{
u32 load_word(const u8* table, u32 offset)
{
    u32 value;
    std::memcpy(&value, table + offset, sizeof(value));
    return value;
}
void store_word(u8* table, u32 offset, u32 value)
{
    std::memcpy(table + offset, &value, sizeof(value));
}
}

// Xbox 0x001858E5
void advance_runtime_shader_issue_schedule(u8* table)
{
    store_word(table, 0xD0, 0);
    store_word(table, 0x450, 0x20);
    u32 cursor = 0x380;
    i32 rows = 14;
    do
    {
        i32 channels = 4;
        do
        {
            store_word(table, cursor - 0x380, load_word(table, cursor - 0x1C0));
            store_word(table, cursor - 0x1C0, 0);
            store_word(table, cursor, load_word(table, cursor + 0x1C0));
            const u32 retained = load_word(table, cursor - 0xE0);
            store_word(table, cursor - 0xE0, 0);
            store_word(table, cursor - 0x2A0, retained);
            store_word(table, cursor + 0xE0, load_word(table, cursor + 0x2A0));
            store_word(table, cursor + 0x1C0, 0x20);
            const u32 next_reason = cursor + 0x2A0;
            cursor += 4;
            --channels;
            store_word(table, next_reason, 0x20);
        } while (channels != 0);
        --rows;
    } while (rows != 0);
}

// Xbox 0x001857E5
u32 runtime_shader_vector_latency(const u32* instruction)
{
    if ((instruction[2] & 0xE0000000) == 0)
    {
        // Retail words at DAT_001A2360.
        constexpr u32 latency[16] = {4, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 4, 4, 4};
        return latency[(instruction[2] >> 25) & 15];
    }
    return 0;
}

// Xbox 0x0018582C
void write_runtime_shader_register_schedule(u8* table, u32 latency, u8 reg, u8 mask, u32 issued)
{
    i32 lane = 0;
    do
    {
        if ((static_cast<u32>(mask) & (1u << lane)) != 0)
        {
            store_word(table, (lane + static_cast<u32>(reg) * 4) * 4, issued + 6);
            store_word(table, (lane + 0x34 + static_cast<u32>(reg) * 4) * 4, latency);
        }
        ++lane;
    } while (lane < 4);
}

// Xbox 0x00187718
u32 runtime_shader_swizzle_channel_mask(u8 w, u8 z, u8 y, u8 x)
{
    return (1u << ((3u - w) & 31)) | (1u << ((3u - z) & 31))
        | (1u << ((3u - y) & 31)) | (1u << ((3u - x) & 31));
}

// Xbox 0x00187760
u8 runtime_shader_operand_channel_mask(const u32* instruction, u32 operand)
{
    u32 word;
    if (operand == 0)
    {
        word = instruction[2];
    }
    else
    {
        if (operand == 1)
        {
            word = instruction[1];
            return static_cast<u8>(runtime_shader_swizzle_channel_mask((word >> 25) & 3,
                (word >> 23) & 3, (word >> 21) & 3, (word >> 19) & 3));
        }
        if (operand - 2u != 0)
        {
            return 0;
        }
        word = instruction[1];
    }
    return static_cast<u8>(runtime_shader_swizzle_channel_mask((word >> 10) & 3,
        (word >> 8) & 3, (word >> 6) & 3, (word >> 4) & 3));
}
}
