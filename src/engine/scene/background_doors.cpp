#include "background_doors.h"

#include <bit>
#include <cstring>

namespace gbtg
{
// Xbox 0x00043B40
void register_background_doors(std::span<const u8> cpu, RuntimeDoorContext& context)
{
    i32 index = 0;
    u32 offset = 0;
    do
    {
        const u32 count = read_u32(cpu, 4);
        const u32 table = read_u32(cpu, 0);
        i32 doors = 0;
        for (u32 row = 0; row < count; ++row)
        {
            if (read_u32(cpu, table + row * 8) == 0xF)
            {
                const u32 block = read_u32(cpu, table + row * 8 + 4);
                if (block != 0)
                {
                    doors = std::bit_cast<i32>(read_u32(cpu, block));
                }
                break;
            }
        }
        if (index >= doors)
        {
            return;
        }
        u32 door = 0;
        for (u32 row = 0; row < count; ++row)
        {
            if (read_u32(cpu, table + row * 8) == 0xF)
            {
                const u32 block = read_u32(cpu, table + row * 8 + 4);
                if (block != 0)
                {
                    door = block + 4 + offset;
                }
                break;
            }
        }
        Vec3 position;
        Vec3 euler;
        Vec3 scale;
        std::memcpy(&position, cpu.data() + door, 12);
        std::memcpy(&euler, cpu.data() + door + 0xC, 12);
        std::memcpy(&scale, cpu.data() + door + 0x18, 12);
        register_runtime_door(position, euler, scale,
            std::bit_cast<i32>(read_u32(cpu, door + 0x24)), read_u16(cpu, door + 0x28), context);
        ++index;
        offset += 0x2C;
    }
    while (true);
}
// Xbox 0x00044300
void register_selected_background_doors(std::span<const u8> cpu, std::span<const u8> message,
    RuntimeDoorContext& context)
{
    i32 index = 0;
    u32 offset = 0;
    do
    {
        const u32 count = read_u32(cpu, 4);
        const u32 table = read_u32(cpu, 0);
        i32 doors = 0;
        for (u32 row = 0; row < count; ++row)
        {
            if (read_u32(cpu, table + row * 8) == 0xF)
            {
                const u32 block = read_u32(cpu, table + row * 8 + 4);
                if (block != 0)
                {
                    doors = std::bit_cast<i32>(read_u32(cpu, block));
                }
                break;
            }
        }
        if (index >= doors)
        {
            return;
        }
        u32 door = 0;
        for (u32 row = 0; row < count; ++row)
        {
            if (read_u32(cpu, table + row * 8) == 0xF)
            {
                const u32 block = read_u32(cpu, table + row * 8 + 4);
                if (block != 0)
                {
                    door = block + 4 + offset;
                }
                break;
            }
        }
        Vec3 position;
        Vec3 euler;
        Vec3 scale;
        std::memcpy(&position, cpu.data() + door, 12);
        std::memcpy(&euler, cpu.data() + door + 0xC, 12);
        std::memcpy(&scale, cpu.data() + door + 0x18, 12);
        if (read_u32(cpu, door + 0x28) == u32(read_u16(message, 4)))
        {
            register_runtime_door(position, euler, scale,
                std::bit_cast<i32>(read_u32(cpu, door + 0x24)), read_u16(cpu, door + 0x28), context);
        }
        ++index;
        offset += 0x2C;
    }
    while (true);
}
}
