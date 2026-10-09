#include "shader_rewrite_storage.h"
#include <cstring>

namespace gbtg
{
namespace
{
void store_rewrite_word(u8* address, u32 value)
{
    std::memcpy(address, &value, sizeof(value));
}
}

// Xbox 0x00186679
u32 initialize_runtime_shader_rewrite_storage(u8* storage, u32 options)
{
    store_rewrite_word(storage + 0x204, options);
    u8* entry = storage;
    i32 count = 64;
    do
    {
        store_rewrite_word(entry, 0);
        entry += 4;
        --count;
    } while (count != 0);
    entry = storage + 0x100;
    count = 64;
    do
    {
        store_rewrite_word(entry, 0);
        entry += 4;
        --count;
    } while (count != 0);
    if (static_cast<u8>(options) != 1)
    {
        entry = storage + 0x20;
        count = 48;
        do
        {
            store_rewrite_word(entry, 0x0F0F0F0Fu);
            entry += 4;
            --count;
        } while (count != 0);
    }
    if ((options & 0x200u) != 0)
    {
        store_rewrite_word(storage + 0xE0, 0x0F0F0F0Fu);
        store_rewrite_word(storage + 0xE4, 0x0F0F0F0Fu);
        store_rewrite_word(storage + 0xE8, 0x0F0F0F0Fu);
    }
    storage[0x200] = 0;
    return 0;
}

// Xbox 0x001862A8
u32 decode_runtime_shader_rewrite_destinations(const u32* instruction, u8* destinations)
{
    store_rewrite_word(destinations, 0);
    destinations[4] = 0;
    destinations[5] = 0;
    destinations[6] = 0;
    const u32 vector_opcode = (instruction[2] >> 25) & 15u;
    if (vector_opcode != 0)
    {
        if (vector_opcode == 13)
        {
            destinations[4] = 8;
            destinations[3] = 0xF0;
        }
        if (((instruction[0] >> 24) & 15u) != 0)
        {
            destinations[4] = static_cast<u8>((instruction[0] >> 24) & 15u);
            destinations[3] = static_cast<u8>(((instruction[0] >> 20) & 15u) - 0x20u);
        }
        if ((instruction[0] & 4u) == 0 && (instruction[0] & 0xF000u) != 0)
        {
            destinations[0] = 1;
            destinations[2] = static_cast<u8>((instruction[0] >> 12) & 15u);
            u8 index = static_cast<u8>(instruction[0] >> 3);
            if (((instruction[0] >> 3) & 0x100u) == 0)
            {
                index = static_cast<u8>(index + 0x20u);
            }
            else
            {
                index = static_cast<u8>(index + 0x10u);
            }
            destinations[1] = index;
        }
    }
    if ((instruction[2] & 0xE0000000u) != 0)
    {
        if ((instruction[0] & 4u) != 0 && (instruction[0] & 0xF000u) != 0)
        {
            destinations[0] = 0;
            destinations[2] = static_cast<u8>((instruction[0] >> 12) & 15u);
            u8 index = static_cast<u8>(instruction[0] >> 3);
            if (((instruction[0] >> 3) & 0x100u) == 0)
            {
                index = static_cast<u8>(index + 0x20u);
            }
            else
            {
                index = static_cast<u8>(index + 0x10u);
            }
            destinations[1] = index;
        }
        if (((instruction[0] >> 16) & 15u) != 0)
        {
            destinations[6] = static_cast<u8>((instruction[0] >> 16) & 15u);
            if ((instruction[2] & 0x1E000000u) == 0)
            {
                destinations[5] = static_cast<u8>(((instruction[0] >> 20) & 15u) - 0x20u);
            }
            else
            {
                destinations[5] = 0xE1;
            }
        }
    }
    return 0;
}

// Xbox 0x00186394
u32 apply_runtime_shader_rewrite_destinations(u32* instruction, const u8* destinations)
{
    const u32 vector_opcode = (instruction[2] >> 25) & 15u;
    u32 value;
    if (vector_opcode != 0)
    {
        u8 mask;
        if (destinations[0] == 0)
        {
            mask = 0;
        }
        else
        {
            mask = destinations[2];
        }
        if (destinations[4] == 0 && mask == 0)
        {
            reinterpret_cast<u8*>(instruction)[3] &= 0xF0u;
            instruction[2] &= 0xE1FFFFFFu;
            if ((instruction[0] & 4u) == 0)
            {
                instruction[0] = (instruction[0] & 0xFFFF0FFFu) | 0xFF8u;
            }
        }
        else
        {
            value = instruction[0];
            if ((value & 4u) == 0 && (value & 0xF000u) != 0)
            {
                instruction[0] = ((static_cast<u32>(destinations[2]) << 12) ^ value) & 0xF000u ^ value;
            }
            value = instruction[0];
            if ((value & 0xF000000u) != 0)
            {
                instruction[0] = ((static_cast<u32>(destinations[4]) << 24) ^ value) & 0xF000000u ^ value;
            }
        }
    }
    const u32 scalar_opcode = instruction[2];
    if ((scalar_opcode & 0xE0000000u) == 0)
    {
        return 0;
    }
    u8 mask;
    if (destinations[0] == 0)
    {
        mask = destinations[2];
    }
    else
    {
        mask = 0;
    }
    if (destinations[6] == 0 && mask == 0)
    {
        reinterpret_cast<u8*>(instruction)[2] &= 0xF0u;
        instruction[2] = scalar_opcode & 0x1FFFFFFFu;
        if ((instruction[0] & 4u) == 0)
        {
            return 0;
        }
        value = (instruction[0] & 0xFFFF0FFFu) | 0xFF8u;
    }
    else
    {
        value = instruction[0];
        if ((value & 4u) != 0 && (value & 0xF000u) != 0)
        {
            instruction[0] = ((static_cast<u32>(destinations[2]) << 12) ^ value) & 0xF000u ^ value;
        }
        value = instruction[0];
        if ((value & 0xF0000u) == 0)
        {
            return 0;
        }
        value ^= ((static_cast<u32>(destinations[6]) << 16) ^ value) & 0xF0000u;
        instruction[0] = value;
        if (vector_opcode == 0)
        {
            return 0;
        }
        if ((scalar_opcode & 0x1E000000u) != 0)
        {
            return 0;
        }
        value = (value & 0xFF1FFFFFu) | 0x100000u;
    }
    instruction[0] = value;
    return 0;
}
}
