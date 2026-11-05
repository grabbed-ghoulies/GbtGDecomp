#include "effect_simulation.h"
#include "effect_channels.h"
#include "effect_commands.h"
#include "effect_command_selection.h"
#include "../entities/bounded_channels.h"
#include "../entities/entity.h"
#include "effect_particles.h"

#include <bit>

namespace gbtg
{
// Xbox 0x000F18E0
u32 simulate_actor_effect_particle(EffectParticle& particle, ActorEffectNode& owner,
    float step, u32& collided, float& previous_collisions,
    RuntimeEffectSimulation& context)
{
    const float life = particle.life;
    if (life != -1.f)
    {
        const float remaining = life - step;
        particle.life = remaining;
        if (0.f >= remaining)
        {
            return 0;
        }
    }
    const u32 kind = get(owner.bytes, actor_effect_field::draw_mode);
    if (kind == 0 || kind == 2)
    {
        if ((particle.frames.mode & 2) != 0)
        {
            decrease_runtime_bounded_channel(particle.frames, step);
        }
        else if ((particle.frames.mode & 1) != 0)
        {
            increase_runtime_bounded_channel(particle.frames, step);
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
    previous_collisions = std::bit_cast<float>(particle.collisions);
    approach_actor_effect_channels(particle.colour_channels, context.movement.clock);
    const float target = particle.target_size;
    const float difference = target - particle.size;
    const float distance = engine_absolute(difference);
    const float rate = particle.size_rate;
    const float change = rate * step;
    if (rate != 0.f)
    {
        if (change > distance)
        {
            particle.size_rate = 0.f;
            particle.size = target;
        }
        else if (0.f > difference)
        {
            particle.size = particle.size - change;
        }
        else
        {
            particle.size = particle.size + change;
        }
    }
    if ((particle.channel_flags & 4) != 0)
    {
        tick_actor_effect_oscillation(particle.oscillation, step);
    }
    if ((particle.channel_flags & 8) != 0)
    {
        tick_runtime_bounded_channels(particle.bounded_channels, step, nullptr);
    }
    if (particle.unk_108 > 0.f)
    {
        particle.unk_108 = particle.unk_108 - step;
    }
    if (get(owner.bytes, actor_effect_field::unk_BC) != -1.f)
    {
        const Vec3 origin = get(owner.bytes, actor_effect_field::position);
        const float x = particle.position.x - origin.x;
        const float y = particle.position.y - origin.y;
        const float z = particle.position.z - origin.z;
        const float squared = (x * x + y * y) + z * z;
        if (squared > get(owner.bytes, actor_effect_field::unk_BC))
        {
            const u32 fallback = find_actor_effect_fallback_command(particle, owner, context.resolve);
            if (fallback != 0)
            {
                particle.command = fallback;
            }
        }
    }
    u32 cursor = particle.command;
    const bool alive = execute_actor_effect_commands(particle, owner, cursor, context.resolve,
        context.commands.effects.random_state, context.pal, context.override_pal);
    particle.command = cursor;
    return alive;
}
}
