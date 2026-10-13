#include "effect_command_selection.h"
#include "effect_channels.h"
#include <bit>
#include <cstring>

namespace gbtg
{
// Xbox 0x000F0300
bool actor_effect_command_condition(std::span<const u8> command, std::span<const u8> particle)
{
    switch (read_u32(command, 8))
    {
    case 0:
        if (read_f32(particle, 0x1C) >= 0.f)
        {
            return true;
        }
        break;
    case 1:
        if (read_f32(particle, 0x1C) >= -20.f)
        {
            return true;
        }
        break;
    case 2:
        if (actor_effect_channels_are_active(particle.subspan(0x60)))
        {
            return true;
        }
        break;
    case 3:
        if (std::bit_cast<i32>(read_u32(particle, 0x5C)) < 3)
        {
            return true;
        }
        break;
    case 4:
        if (read_u32(particle, 0x5C) == 0)
        {
            return true;
        }
        break;
    case 5:
        if (read_f32(particle, 0x108) > 0.f)
        {
            return true;
        }
        break;
    }
    return false;
}

// Xbox 0x000F0FF0
u32 find_actor_effect_fallback_command(std::span<const u8> particle, ActorEffectNode& effect,
    const std::function<std::span<const u8>(u32)>& resolve)
{
    u32 address = read_u32(particle, 0x14C);
    auto command = resolve(address);
    u32 previous = 0;
    if (read_u32(command, 4) != 2)
    {
        do
        {
            if (read_u32(command, 4) == 0x1B5E)
            {
                std::memcpy(effect.bytes.data() + 0x138, &previous, 4);
                previous = read_u32(command, 0) + address;
            }
            address += read_u32(command, 0);
            command = resolve(address);
        }
        while (read_u32(command, 4) != 2);
        return read_u32(effect.bytes, 0x138);
    }
    return read_u32(effect.bytes, 0x138);
}
}
