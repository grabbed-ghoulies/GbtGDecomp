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
void set_number(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}
void recycle_particle(ActorEffectNode& effect, std::span<u8> particle)
{
    const u16 head = read_u16(effect.bytes, 6);
    std::memcpy(particle.data() + 0x5A, &head, 2);
    const i32 displacement = i32(particle.data() - effect.bytes.data()) - 0x390;
    const u16 index = u16(displacement / 0x150);
    const u16 active = u16(read_u16(effect.bytes, 0) - 1);
    std::memcpy(effect.bytes.data(), &active, 2);
    std::memcpy(effect.bytes.data() + 6, &index, 2);
}
void advance_emitter_channels(ActorEffectNode& effect, float step, bool divided)
{
    for (size_t axis = 0; axis < 3; ++axis)
    {
        const size_t offset = axis * 4;
        set_number(effect.bytes, 0x5C + offset,
            read_f32(effect.bytes, 0x68 + offset) * step + read_f32(effect.bytes, 0x5C + offset));
    }
    set_number(effect.bytes, 0xA0, read_f32(effect.bytes, 0xAC) * step + read_f32(effect.bytes, 0xA0));
    // The divided path reverses MULSS operands for the middle channel.
    const float middle = divided ? step * read_f32(effect.bytes, 0xB0) : read_f32(effect.bytes, 0xB0) * step;
    set_number(effect.bytes, 0xA4, middle + read_f32(effect.bytes, 0xA4));
    set_number(effect.bytes, 0xA8, read_f32(effect.bytes, 0xB4) * step + read_f32(effect.bytes, 0xA8));
}
}

// Xbox 0x000EF260
void spawn_and_simulate_actor_effect_particle(ActorEffectNode& effect, float step,
    RuntimeEffectTickContext& context)
{
    auto& simulation = context.simulation;
    u32& random = simulation.commands.effects.random_state;
    u8* allocated = allocate_actor_effect_particle(effect, context.address(effect.bytes.data()),
        read_u32(effect.bytes, 0x134), random, simulation.pal, simulation.override_pal);
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
    if (i16(read_u16(effect.bytes, 2)) > 0)
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
        while (i32(index) < i16(read_u16(effect.bytes, 2)));
    }
}

// Xbox 0x000EF2D0
bool tick_actor_effect(ActorEffectNode& effect, float step,
    RuntimeEffectTickContext& context)
{
    const float duration = read_f32(effect.bytes, 0x10);
    if (duration != -1.f)
    {
        if (duration > 0.f)
        {
            set_number(effect.bytes, 0x10, duration - step);
        }
        else
        {
            set_number(effect.bytes, 8, -1.f);
        }
    }
    if ((effect.bytes[0x18] & 0x80) != 0)
    {
        float yaw, pitch;
        const Vec3& direction = context.camera_direction;
        direction_to_yaw_pitch(direction.x, direction.y, direction.z, yaw, pitch);
        set_number(effect.bytes, 0x50, pitch);
        set_number(effect.bytes, 0x54, yaw);
        set_number(effect.bytes, 0x58, 0.f);
        set_number(effect.bytes, 0xA0, pitch);
        set_number(effect.bytes, 0xA4, yaw);
        set_number(effect.bytes, 0xA8, 0.f);
    }
    std::memcpy(effect.bytes.data() + 0x370, effect.bytes.data() + 0x1C, 12);
    if (read_u32(effect.bytes, 0x14) == 0)
    {
        const u32 first = 1;
        std::memcpy(effect.bytes.data() + 0x14, &first, 4);
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
            const bool stationary = read_f32(effect.bytes, 0x28) == read_f32(effect.bytes, 0x1C)
                && read_f32(effect.bytes, 0x2C) == read_f32(effect.bytes, 0x20)
                && read_f32(effect.bytes, 0x30) == read_f32(effect.bytes, 0x24);
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
                const Vec3 position{read_f32(effect.bytes, 0x1C), read_f32(effect.bytes, 0x20), read_f32(effect.bytes, 0x24)};
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
                        set_number(effect.bytes, 0x1C, read_f32(effect.bytes, 0x28) * complement + position.x * fraction);
                        set_number(effect.bytes, 0x20, position.y * fraction + complement * read_f32(effect.bytes, 0x2C));
                        set_number(effect.bytes, 0x24, position.z * fraction + complement * read_f32(effect.bytes, 0x30));
                        prepare_actor_effect_binding(effect, step, context.address);
                        advance_emitter_channels(effect, substep, true);
                        spawn_and_simulate_actor_effect_particle(effect, substep, context);
                        ++iteration;
                    }
                    while (count > float(iteration));
                }
                std::memcpy(effect.bytes.data() + 0x1C, &position, 12);
            }
        }
    }
    tick_active_actor_effect_particles(effect, step, context);
    if ((effect.bytes[0x18] & 8) != 0
        && (read_u16(effect.bytes, 4) == read_u16(effect.bytes, 2)
            || (read_f32(effect.bytes, 8) == -1.f && i16(read_u16(effect.bytes, 0)) == i16(read_u16(effect.bytes, 4)))))
    {
        return false;
    }
    std::memcpy(effect.bytes.data() + 0x28, effect.bytes.data() + 0x1C, 12);
    return true;
}
}
