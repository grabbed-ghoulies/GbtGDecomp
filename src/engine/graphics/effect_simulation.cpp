#include "effect_simulation.h"
#include "effect_channels.h"
#include "effect_commands.h"
#include "effect_command_selection.h"
#include "../entities/bounded_channels.h"
#include "../entities/entity.h"

#include <cstring>

namespace gbtg
{
namespace
{
void write_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}
}

// Xbox 0x000F18E0
u32 simulate_actor_effect_particle(std::span<u8> particle, ActorEffectNode& owner,
    float step, u32& collided, float& previous_collisions,
    RuntimeEffectSimulation& context)
{
    const float life = read_f32(particle, 0xA8);
    if (life != -1.f)
    {
        const float remaining = life - step;
        write_float(particle, 0xA8, remaining);
        if (0.f >= remaining)
        {
            return 0;
        }
    }
    const u32 kind = read_u32(owner.bytes, 0x13C);
    if (kind == 0 || kind == 2)
    {
        if ((particle[0x104] & 2) != 0)
        {
            decrease_runtime_bounded_channel(particle.subspan(0xF4, 0x14), step);
        }
        else if ((particle[0x104] & 1) != 0)
        {
            increase_runtime_bounded_channel(particle.subspan(0xF4, 0x14), step);
        }
    }
    const bool hit = move_actor_effect_particle(particle, context.movement, [&]()
    {
        notify_effect_particle_collision(particle, owner, context.commands);
    });
    if (hit)
    {
        collided = 1;
        return context.commands.callback_result;
    }
    collided = 0;
    std::memcpy(&previous_collisions, particle.data() + 0x5C, 4);
    approach_actor_effect_channels(particle.subspan(0x60, 0x3C), context.movement.clock);
    const float target = read_f32(particle, 0xA0);
    const float difference = target - read_f32(particle, 0x48);
    const float distance = engine_absolute(difference);
    const float rate = read_f32(particle, 0x9C);
    const float change = rate * step;
    if (rate != 0.f)
    {
        if (change > distance)
        {
            write_float(particle, 0x9C, 0.f);
            write_float(particle, 0x48, target);
        }
        else if (0.f > difference)
        {
            write_float(particle, 0x48, read_f32(particle, 0x48) - change);
        }
        else
        {
            write_float(particle, 0x48, read_f32(particle, 0x48) + change);
        }
    }
    if ((particle[0x58] & 4) != 0)
    {
        tick_actor_effect_oscillation(particle.subspan(0xB8, 0x3C), step);
    }
    if ((particle[0x58] & 8) != 0)
    {
        tick_runtime_bounded_channels(particle.subspan(0x10C, 0x3C), step, nullptr);
    }
    if (read_f32(particle, 0x108) > 0.f)
    {
        write_float(particle, 0x108, read_f32(particle, 0x108) - step);
    }
    if (read_f32(owner.bytes, 0xBC) != -1.f)
    {
        const float x = read_f32(particle, 0) - read_f32(owner.bytes, 0x1C);
        const float y = read_f32(particle, 4) - read_f32(owner.bytes, 0x20);
        const float z = read_f32(particle, 8) - read_f32(owner.bytes, 0x24);
        const float squared = (x * x + y * y) + z * z;
        if (squared > read_f32(owner.bytes, 0xBC))
        {
            const u32 fallback = find_actor_effect_fallback_command(particle, owner, context.resolve);
            if (fallback != 0)
            {
                std::memcpy(particle.data() + 0x14C, &fallback, 4);
            }
        }
    }
    u32 cursor = read_u32(particle, 0x14C);
    const bool alive = execute_actor_effect_commands(particle, owner, cursor, context.resolve,
        context.commands.effects.random_state, context.pal, context.override_pal);
    std::memcpy(particle.data() + 0x14C, &cursor, 4);
    return alive;
}
}
