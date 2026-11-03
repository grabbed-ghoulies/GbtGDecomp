#include "effect_collision_commands.h"
#include "../camera/actor_camera_effects.h"
#include "../entities/player_mind_events.h"
#include "../entities/entity.h"
#include "../runtime/random_state.h"
#include "effect_particles.h"

#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
bool within_collision_range(std::span<const u8> command, size_t offset, i32 count)
{
    const i32 lower = i32(read_u32(command, offset));
    return lower == 0 || (count >= lower && count < i32(read_u32(command, offset + 4)));
}
bool collision_chance(std::span<const u8> command, size_t offset, u32& random)
{
    const float fraction = advance_runtime_random_fraction(random);
    return read_f32(command, offset) > fraction;
}
}

// Xbox 0x000F10F0
bool execute_effect_collision_commands(EffectParticle& particle,
    const ActorEffectNode& owner, RuntimeEffectCollisionCommands& context)
{
    u32 mask = 0;
    auto command = owner.script;
    u32& random = context.effects.random_state;
    while (true)
    {
        const i32 count = i32(particle.collisions);
        switch (read_u32(command, 4))
        {
        case 0x1B74:
            command = command.subspan(0x4C);
            break;
        case 0x1B75:
        case 0x1B7C:
        {
            if (within_collision_range(command, 0xC, count))
            {
                if (collision_chance(command, 0x14, random))
                {
                    g_actor_creation_options.flags = 0;
                    Vec3 position, euler;
                    sample_effect_collision_pose(particle.position, context.collision.normal, position, euler);
                    if (read_u32(command, 0x98) != 0)
                    {
                        const bool directed = runtime_direction_angle(particle.velocity, euler.y);
                        if (read_u32(command, 4) == 0x1B75 && !directed)
                        {
                            euler.y = advance_runtime_random_fraction(random) * std::bit_cast<float>(0x40C90FDBu);
                        }
                    }
                    g_actor_creation_options.position = position;
                    g_actor_creation_options.euler = euler;
                    g_actor_creation_options.flags |= 5;
                    if (load_actor_effect_stream(reinterpret_cast<const char*>(command.data() + 0x18),
                        &g_actor_creation_options, context.effects) != 0)
                    {
                        const i32 index = g_last_actor_effect;
                        g_last_actor_effect = -1;
                        if (index != -1)
                        {
                            g_actor_effect_nodes[index]->bytes[0x18] |= 8;
                        }
                    }
                }
                mask |= read_u32(command, 0x9C);
            }
            command = command.subspan(0xA0);
            break;
        }
        case 0x1B76:
        {
            if (collision_chance(command, 0x18, random))
            {
                const u8 kind = context.collision.kind == 0 ? 0
                    : u8(1u << ((context.collision.kind - 1u) & 31u));
                if ((command[0xC] & kind) != 0 && within_collision_range(command, 0x10, count))
                {
                    Vec3 position, euler;
                    sample_effect_collision_pose(particle.position, context.collision.normal, position, euler);
                    spawn_effect_debris(i32(read_u32(command, 0x20)), position, euler, context.debris);
                    mask |= read_u32(command, 0x1C);
                }
            }
            command = command.subspan(0x24);
            break;
        }
        case 0x1B77:
            if (collision_chance(command, 0x14, random) && context.collision.kind == 2
                && within_collision_range(command, 0xC, count))
            {
                mask |= read_u32(command, 0x18);
            }
            command = command.subspan(0x1C);
            break;
        case 0x1B78:
            if (within_collision_range(command, 0xC, count) && collision_chance(command, 0x14, random))
            {
                if (read_u32(command, 0x24) != 0)
                {
                    particle.motion_flags |= 4;
                }
                particle.bounce = read_f32(command, 0x18);
                particle.spin_bounce = read_f32(command, 0x1C);
                const float scale = read_f32(command, 0x20);
                for (BoundedChannel& channel : particle.bounded_channels)
                {
                    channel.lower = channel.lower * scale;
                    channel.upper = channel.upper * scale;
                    channel.value = channel.value * scale;
                }
            }
            command = command.subspan(0x28);
            break;
        case 0x1B79:
            if (within_collision_range(command, 0xC, count) && collision_chance(command, 0x14, random))
            {
                request_positioned_audio_event_voice(reinterpret_cast<const char*>(command.data() + 0x18),
                    1, particle.position, context.voices);
            }
            command = command.subspan(0x98);
            break;
        case 0x1B7A:
        {
            const i32 lower = i32(read_u32(command, 0xC));
            if ((lower == 0 || count >= lower) && collision_chance(command, 0x10, random))
            {
                mask = 1;
            }
            command = command.subspan(0x14);
            break;
        }
        case 0x1B7B:
            if (within_collision_range(command, 0xC, count) && collision_chance(command, 0x14, random))
            {
                char name[128];
                format_footstep_sound_name(name, context.collision.surface, "", "default", context.footsteps);
                if (name[0] != '\0')
                {
                    request_positioned_audio_event_voice(name, 1, particle.position, context.voices);
                }
            }
            command = command.subspan(0x18);
            break;
        case 0x1B7D:
            if (within_collision_range(command, 0xC, count))
            {
                if (collision_chance(command, 0x14, random))
                {
                    Vec3 position, euler;
                    sample_effect_collision_pose(particle.position, context.collision.normal, position, euler);
                    load_actor_camera_shake(reinterpret_cast<const char*>(command.data() + 0x18), position, 1.f,
                        context.effects.models.registry, context.effects.models.error);
                }
                mask |= read_u32(command, 0x98);
            }
            command = command.subspan(0x9C);
            break;
        default:
            return mask == 0;
        }
    }
}

// Xbox 0x000F18C0
void notify_effect_particle_collision(EffectParticle& particle,
    const ActorEffectNode& owner, RuntimeEffectCollisionCommands& context)
{
    particle.collisions = particle.collisions + 1u;
    context.callback_result = execute_effect_collision_commands(particle, owner, context);
}
}
