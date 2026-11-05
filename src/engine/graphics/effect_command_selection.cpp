#include "effect_command_selection.h"
#include "effect_channels.h"
#include "effect_particles.h"
#include <bit>
#include <cstring>

namespace gbtg
{
// Xbox 0x000F0300
bool actor_effect_command_condition(std::span<const u8> command, const EffectParticle& particle)
{
    switch (read_u32(command, 8))
    {
    case 0:
        if (particle.velocity.y >= 0.f)
        {
            return true;
        }
        break;
    case 1:
        if (particle.velocity.y >= -20.f)
        {
            return true;
        }
        break;
    case 2:
        if (actor_effect_channels_are_active(particle.colour_channels))
        {
            return true;
        }
        break;
    case 3:
        if (std::bit_cast<i32>(particle.collisions) < 3)
        {
            return true;
        }
        break;
    case 4:
        if (particle.collisions == 0)
        {
            return true;
        }
        break;
    case 5:
        if (particle.unk_108 > 0.f)
        {
            return true;
        }
        break;
    }
    return false;
}

// Xbox 0x000F0FF0
u32 find_actor_effect_fallback_command(const EffectParticle& particle, ActorEffectNode& effect,
    const std::function<std::span<const u8>(u32)>& resolve)
{
    u32 address = particle.command;
    auto command = resolve(address);
    u32 previous = 0;
    if (read_u32(command, 4) != 2)
    {
        do
        {
            if (read_u32(command, 4) == 0x1B5E)
            {
                set(effect.bytes, actor_effect_field::command, previous);
                previous = read_u32(command, 0) + address;
            }
            address += read_u32(command, 0);
            command = resolve(address);
        }
        while (read_u32(command, 4) != 2);
        return get(effect.bytes, actor_effect_field::command);
    }
    return get(effect.bytes, actor_effect_field::command);
}
}
