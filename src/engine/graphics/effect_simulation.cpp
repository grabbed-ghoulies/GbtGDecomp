#include "effect_simulation.h"
#include "effect_channels.h"
#include "effect_commands.h"
#include "effect_command_selection.h"
#include "../entities/bounded_channels.h"
#include "../entities/entity.h"
#include "effect_particles.h"

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
    const float life = get(particle, effect_particle_field::life);
    if (life != -1.f)
    {
        const float remaining = life - step;
        set(particle, effect_particle_field::life, remaining);
        if (0.f >= remaining)
        {
            return 0;
        }
    }
    const u32 kind = read_u32(owner.bytes, 0x13C);
    if (kind == 0 || kind == 2)
    {
        if ((get(particle, effect_particle_field::frame_mode) & 2) != 0)
        {
            decrease_runtime_bounded_channel(particle.subspan(0xF4, 0x14), step);
        }
        else if ((get(particle, effect_particle_field::frame_mode) & 1) != 0)
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
    approach_actor_effect_channels(record(particle, effect_particle_field::colour_channels), context.movement.clock);
    const float target = get(particle, effect_particle_field::target_size);
    const float difference = target - get(particle, effect_particle_field::size);
    const float distance = engine_absolute(difference);
    const float rate = get(particle, effect_particle_field::size_rate);
    const float change = rate * step;
    if (rate != 0.f)
    {
        if (change > distance)
        {
            set(particle, effect_particle_field::size_rate, 0.f);
            set(particle, effect_particle_field::size, target);
        }
        else if (0.f > difference)
        {
            set(particle, effect_particle_field::size, get(particle, effect_particle_field::size) - change);
        }
        else
        {
            set(particle, effect_particle_field::size, get(particle, effect_particle_field::size) + change);
        }
    }
    if ((get(particle, effect_particle_field::channel_flags) & 4) != 0)
    {
        tick_actor_effect_oscillation(particle.subspan(0xB8, 0x3C), step);
    }
    if ((get(particle, effect_particle_field::channel_flags) & 8) != 0)
    {
        tick_runtime_bounded_channels(particle.subspan(0x10C, 0x3C), step, nullptr);
    }
    if (get(particle, effect_particle_field::unk_108) > 0.f)
    {
        set(particle, effect_particle_field::unk_108, get(particle, effect_particle_field::unk_108) - step);
    }
    if (read_f32(owner.bytes, 0xBC) != -1.f)
    {
        const float x = get(particle, effect_particle_field::position).x - read_f32(owner.bytes, 0x1C);
        const float y = get(particle, effect_particle_field::position).y - read_f32(owner.bytes, 0x20);
        const float z = get(particle, effect_particle_field::position).z - read_f32(owner.bytes, 0x24);
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
    u32 cursor = get(particle, effect_particle_field::command);
    const bool alive = execute_actor_effect_commands(particle, owner, cursor, context.resolve,
        context.commands.effects.random_state, context.pal, context.override_pal);
    std::memcpy(particle.data() + 0x14C, &cursor, 4);
    return alive;
}
}
