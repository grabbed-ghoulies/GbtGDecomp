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
void recycle_particle(ActorEffectNode& effect, std::span<u8> particle)
{
    const u16 head = get(effect.bytes, actor_effect_field::free_head);
    set(particle, effect_particle_field::next_free, head);
    const i32 displacement = i32(particle.data() - effect.bytes.data()) - i32(actor_effect_field::particles_offset);
    const u16 index = u16(displacement / i32(effect_particle_field::kSize));
    const u16 active = u16(get(effect.bytes, actor_effect_field::used_count) - 1);
    set(effect.bytes, actor_effect_field::used_count, active);
    set(effect.bytes, actor_effect_field::free_head, index);
}
void advance_emitter_channels(ActorEffectNode& effect, float step, bool divided)
{
    float* const spin = &field_data(effect.bytes, actor_effect_field::spin)->x;
    const Vec3 spin_rate = get(effect.bytes, actor_effect_field::spin_rate);
    const float* const rates = &spin_rate.x;
    for (size_t axis = 0; axis < 3; ++axis)
    {
        spin[axis] = rates[axis] * step + spin[axis];
    }
    Vec3* const euler = field_data(effect.bytes, actor_effect_field::volume_euler);
    const Vec3 rate = get(effect.bytes, actor_effect_field::volume_spin_rate);
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
    u8* allocated = allocate_actor_effect_particle(effect, context.address(effect.bytes.data()),
        get(effect.bytes, actor_effect_field::script), random, simulation.pal, simulation.override_pal);
    if (allocated != nullptr)
    {
        const std::span<u8> particle{allocated, 0x150};
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
    if (i16(get(effect.bytes, actor_effect_field::capacity)) > 0)
    {
        u32 index = 0;
        do
        {
            const auto particle = effect.bytes.subspan(size_t(index) * 0x150 + 0x390, 0x150);
            if (i16(get(particle, effect_particle_field::next_free)) == -2)
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
                    const Vec3 position = get(particle, effect_particle_field::position);
                    auto& commands = context.simulation.commands;
                    const float fraction = advance_runtime_random_fraction(commands.effects.random_state);
                    notify_actor_effect_particle(effect, .5f > fraction, position, context.frame,
                        context.last_sound_frame, commands.voices, commands.effects);
                }
            }
            ++iteration;
            index = iteration & 0xFFFFu;
        }
        while (i32(index) < i16(get(effect.bytes, actor_effect_field::capacity)));
    }
}

// Xbox 0x000EF2D0
bool tick_actor_effect(ActorEffectNode& effect, float step,
    RuntimeEffectTickContext& context)
{
    const float duration = get(effect.bytes, actor_effect_field::life);
    if (duration != -1.f)
    {
        if (duration > 0.f)
        {
            set(effect.bytes, actor_effect_field::life, duration - step);
        }
        else
        {
            set(effect.bytes, actor_effect_field::emitting, -1.f);
        }
    }
    if ((get(effect.bytes, actor_effect_field::flags) & 0x80) != 0)
    {
        float yaw, pitch;
        const Vec3& direction = context.camera_direction;
        direction_to_yaw_pitch(direction.x, direction.y, direction.z, yaw, pitch);
        set(effect.bytes, actor_effect_field::euler, Vec3{pitch, yaw, 0.f});
        set(effect.bytes, actor_effect_field::volume_euler, Vec3{pitch, yaw, 0.f});
    }
    set(effect.bytes, actor_effect_field::tick_position, get(effect.bytes, actor_effect_field::position));
    if (get(effect.bytes, actor_effect_field::ticks) == 0)
    {
        set(effect.bytes, actor_effect_field::ticks, 1);
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
            const Vec3 previous = get(effect.bytes, actor_effect_field::previous_position);
            const Vec3 current = get(effect.bytes, actor_effect_field::position);
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
                const Vec3 position = get(effect.bytes, actor_effect_field::position);
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
                        field_data(effect.bytes, actor_effect_field::position)->x =
                            get(effect.bytes, actor_effect_field::previous_position).x * complement +
                            position.x * fraction;
                        field_data(effect.bytes, actor_effect_field::position)->y =
                            position.y * fraction +
                            complement * get(effect.bytes, actor_effect_field::previous_position).y;
                        field_data(effect.bytes, actor_effect_field::position)->z =
                            position.z * fraction +
                            complement * get(effect.bytes, actor_effect_field::previous_position).z;
                        prepare_actor_effect_binding(effect, step, context.address);
                        advance_emitter_channels(effect, substep, true);
                        spawn_and_simulate_actor_effect_particle(effect, substep, context);
                        ++iteration;
                    }
                    while (count > float(iteration));
                }
                set(effect.bytes, actor_effect_field::position, position);
            }
        }
    }
    tick_active_actor_effect_particles(effect, step, context);
    if ((get(effect.bytes, actor_effect_field::flags) & 8) != 0 &&
        (get(effect.bytes, actor_effect_field::free_count) == get(effect.bytes, actor_effect_field::capacity) ||
         (get(effect.bytes, actor_effect_field::emitting) == -1.f &&
          i16(get(effect.bytes, actor_effect_field::used_count)) ==
              i16(get(effect.bytes, actor_effect_field::free_count)))))
    {
        return false;
    }
    set(effect.bytes, actor_effect_field::previous_position, get(effect.bytes, actor_effect_field::position));
    return true;
}
}
