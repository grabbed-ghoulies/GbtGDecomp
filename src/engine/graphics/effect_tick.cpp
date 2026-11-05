#include "effect_tick.h"
#include "effect_particles.h"
#include "effect_placement.h"
#include "effect_binding.h"
#include "../camera/entity_camera.h"
#include "../entities/entity.h"
#include "../runtime/random_state.h"

#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
void recycle_particle(ActorEffectNode& effect, EffectParticle& particle)
{
    const u16 head = effect_header(effect.bytes).free_head;
    particle.next_free = head;
    const i32 displacement =
        i32(reinterpret_cast<u8*>(&particle) - effect.bytes.data()) - i32(sizeof(ActorEffectHeader));
    const u16 index = u16(displacement / i32(sizeof(EffectParticle)));
    const u16 active = u16(effect_header(effect.bytes).used_count - 1);
    effect_header(effect.bytes).used_count = active;
    effect_header(effect.bytes).free_head = index;
}
void advance_emitter_channels(ActorEffectNode& effect, float step, bool divided)
{
    float* const spin = &effect_header(effect.bytes).spin.x;
    const Vec3 spin_rate = effect_header(effect.bytes).spin_rate;
    const float* const rates = &spin_rate.x;
    for (size_t axis = 0; axis < 3; ++axis)
    {
        spin[axis] = rates[axis] * step + spin[axis];
    }
    Vec3* const euler = &effect_header(effect.bytes).volume_euler;
    const Vec3 rate = effect_header(effect.bytes).volume_spin_rate;
    euler->x = rate.x * step + euler->x;
    // The divided path reverses MULSS operands for the middle channel.
    const float middle = divided ? step * rate.y : rate.y * step;
    euler->y = middle + euler->y;
    euler->z = rate.z * step + euler->z;
}
}

// Xbox 0x000EF260
void spawn_and_simulate_actor_effect_particle(ActorEffectNode& effect, float step,
    RuntimeEffectTickContext& context)
{
    auto& simulation = context.simulation;
    u32& random = simulation.commands.effects.random_state;
    EffectParticle* allocated = allocate_actor_effect_particle(effect, context.address(effect.bytes.data()),
        effect_header(effect.bytes).script, random, simulation.pal, simulation.override_pal);
    if (allocated != nullptr)
    {
        EffectParticle& particle = *allocated;
        place_actor_effect_particle(effect.bytes, particle, effect.bytes, context.scratch.placement, random);
        u32 collided = 0;
        float previous = 0.f;
        if (simulate_actor_effect_particle(particle, effect, step, collided, previous, simulation) == 0)
        {
            recycle_particle(effect, particle);
        }
    }
}

// Xbox 0x000EF0B0
void tick_active_actor_effect_particles(ActorEffectNode& effect, float step,
    RuntimeEffectTickContext& context)
{
    u32 iteration = 0;
    if (i16(effect_header(effect.bytes).capacity) > 0)
    {
        u32 index = 0;
        do
        {
            EffectParticle& particle = actor_effect_particle(effect, index);
            if (i16(particle.next_free) == -2)
            {
                const u32 alive = simulate_actor_effect_particle(particle, effect, step,
                    context.scratch.collided, context.scratch.previous_collisions, context.simulation);
                if (alive == 0)
                {
                    recycle_particle(effect, particle);
                }
                else if (context.scratch.collided != 0
                    && std::bit_cast<u32>(context.scratch.previous_collisions) == 1)
                {
                    const Vec3 position = particle.position;
                    auto& commands = context.simulation.commands;
                    const float fraction = advance_runtime_random_fraction(commands.effects.random_state);
                    notify_actor_effect_particle(effect, .5f > fraction, position, context.frame,
                        context.last_sound_frame, commands.voices, commands.effects);
                }
            }
            ++iteration;
            index = iteration & 0xFFFFu;
        }
        while (i32(index) < i16(effect_header(effect.bytes).capacity));
    }
}

// Xbox 0x000EF2D0
bool tick_actor_effect(ActorEffectNode& effect, float step,
    RuntimeEffectTickContext& context)
{
    const float duration = effect_header(effect.bytes).life;
    if (duration != -1.f)
    {
        if (duration > 0.f)
        {
            effect_header(effect.bytes).life = duration - step;
        }
        else
        {
            effect_header(effect.bytes).emitting = -1.f;
        }
    }
    if ((effect_header(effect.bytes).flags & 0x80) != 0)
    {
        float yaw, pitch;
        const Vec3& direction = context.camera_direction;
        direction_to_yaw_pitch(direction.x, direction.y, direction.z, yaw, pitch);
        effect_header(effect.bytes).euler = Vec3{pitch, yaw, 0.f};
        effect_header(effect.bytes).volume_euler = Vec3{pitch, yaw, 0.f};
    }
    effect_header(effect.bytes).tick_position = effect_header(effect.bytes).position;
    if (effect_header(effect.bytes).ticks == 0)
    {
        effect_header(effect.bytes).ticks = 1;
    }
    else
    {
        const float count = float(actor_effect_emission_count(effect, step));
        if (count == 0.f)
        {
            advance_emitter_channels(effect, step, false);
        }
        else
        {
            const float substep = step / count;
            const Vec3 previous = effect_header(effect.bytes).previous_position;
            const Vec3 current = effect_header(effect.bytes).position;
            const bool stationary = previous.x == current.x && previous.y == current.y && previous.z == current.z;
            if (stationary)
            {
                i32 iteration = 0;
                while (count > float(iteration))
                {
                    prepare_actor_effect_binding(effect, step, context.address);
                    advance_emitter_channels(effect, substep, true);
                    spawn_and_simulate_actor_effect_particle(effect, substep, context);
                    ++iteration;
                }
            }
            else
            {
                const Vec3 position = effect_header(effect.bytes).position;
                i32 iteration = 0;
                if (count > 0.f)
                {
                    const float reciprocal = 1.f / count;
                    do
                    {
                        float fraction = reciprocal * float(iteration);
                        if (fraction > 1.f)
                        {
                            fraction = 1.f;
                        }
                        else if (0.f > fraction)
                        {
                            fraction = 0.f;
                        }
                        const float complement = 1.f - fraction;
                        effect_header(effect.bytes).position.x =
                            effect_header(effect.bytes).previous_position.x * complement +
                            position.x * fraction;
                        effect_header(effect.bytes).position.y =
                            position.y * fraction +
                            complement * effect_header(effect.bytes).previous_position.y;
                        effect_header(effect.bytes).position.z =
                            position.z * fraction +
                            complement * effect_header(effect.bytes).previous_position.z;
                        prepare_actor_effect_binding(effect, step, context.address);
                        advance_emitter_channels(effect, substep, true);
                        spawn_and_simulate_actor_effect_particle(effect, substep, context);
                        ++iteration;
                    }
                    while (count > float(iteration));
                }
                effect_header(effect.bytes).position = position;
            }
        }
    }
    tick_active_actor_effect_particles(effect, step, context);
    if ((effect_header(effect.bytes).flags & 8) != 0 &&
        (effect_header(effect.bytes).free_count == effect_header(effect.bytes).capacity ||
         (effect_header(effect.bytes).emitting == -1.f &&
          i16(effect_header(effect.bytes).used_count) ==
              i16(effect_header(effect.bytes).free_count))))
    {
        return false;
    }
    effect_header(effect.bytes).previous_position = effect_header(effect.bytes).position;
    return true;
}
}
