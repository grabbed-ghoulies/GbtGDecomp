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
Vec3 vector_at(std::span<const u8> bytes, size_t offset)
{
    return {read_f32(bytes, offset), read_f32(bytes, offset + 4), read_f32(bytes, offset + 8)};
}
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
void put_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}
}

// Xbox 0x000F10F0
bool execute_effect_collision_commands(std::span<u8> particle,
    const ActorEffectNode& owner, RuntimeEffectCollisionCommands& context)
{
    u32 mask = 0;
    auto command = owner.script;
    u32& random = context.effects.random_state;
    while (true)
    {
        const i32 count = i32(get(particle, effect_particle_field::collisions));
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
                    sample_effect_collision_pose(vector_at(particle, 0), context.collision.normal, position, euler);
                    if (read_u32(command, 0x98) != 0)
                    {
                        const bool directed = runtime_direction_angle(vector_at(particle, 0x18), euler.y);
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
                    sample_effect_collision_pose(vector_at(particle, 0), context.collision.normal, position, euler);
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
                    particle[0x56] |= 4;
                }
                std::memcpy(particle.data() + 0x40, command.data() + 0x18, 8);
                const float scale = read_f32(command, 0x20);
                for (size_t offset : {0x10C, 0x110, 0x118, 0x120, 0x124, 0x12C, 0x134, 0x138, 0x140})
                {
                    put_float(particle, offset, read_f32(particle, offset) * scale);
                }
            }
            command = command.subspan(0x28);
            break;
        case 0x1B79:
            if (within_collision_range(command, 0xC, count) && collision_chance(command, 0x14, random))
            {
                request_positioned_audio_event_voice(reinterpret_cast<const char*>(command.data() + 0x18),
                    1, vector_at(particle, 0), context.voices);
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
                    request_positioned_audio_event_voice(name, 1, vector_at(particle, 0), context.voices);
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
                    sample_effect_collision_pose(vector_at(particle, 0), context.collision.normal, position, euler);
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
void notify_effect_particle_collision(std::span<u8> particle,
    const ActorEffectNode& owner, RuntimeEffectCollisionCommands& context)
{
    const u32 count = get(particle, effect_particle_field::collisions) + 1u;
    std::memcpy(particle.data() + 0x5C, &count, 4);
    context.callback_result = execute_effect_collision_commands(particle, owner, context);
}
}
