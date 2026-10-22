#include "body_death_drops.h"
#include "actor_transform.h"
#include "../navigation/spawn_candidates.h"
#include "gbtg/sound_runtime.h"
#include "player_mind_events.h"
#include <bit>
#include <cstring>

namespace gbtg
{
// Xbox 0x000286B0
bool take_runtime_body_drop_names(std::span<u8> actor, char* name, char* variant)
{
    if (read_u32(actor, 0xBA0) != 0x5F646961)
    {
        return false;
    }
    const char* source = reinterpret_cast<const char*>(actor.data() + 0xBA0);
    char character;
    do
    {
        character = *source++;
        *name++ = character;
    } while (character != '\0');
    source = reinterpret_cast<const char*>(actor.data() + 0xC20);
    do
    {
        character = *source++;
        *variant++ = character;
    } while (character != '\0');
    actor[0xBA0] = 0;
    return true;
}

// Xbox 0x00135080
void spawn_runtime_body_death_drop(RuntimeBodyDeathDrops& context)
{
    if (read_u32(context.actor_bytes, 0xBA0) == 0x5F646961)
    {
        const Vec3 position = actor_world_position(context.actor);
        Vec3 euler = actor_world_euler(context.actor);
        euler.x = 0.f;
        euler.z = 0.f;
        const i32 floor = entity_detail::navigation_floor_index(position, -1, 0);
        Vec3 selected;
        if (entity_detail::select_navigation_spawn_position(floor, nullptr, position,
                10.f, 40.f, nullptr, nullptr, 0x19, 0, selected, context.random, context.occupancy_initial) ||
            entity_detail::select_navigation_spawn_position(floor, nullptr, position,
                30.f, 60.f, nullptr, nullptr, 0x19, 0, selected, context.random, context.occupancy_initial))
        {
            const Vec3 direction{position.x - selected.x, position.y - selected.y,
                position.z - selected.z};
            runtime_direction_angle(direction, euler.y);
            char name[0x80];
            char variant[0x80];
            take_runtime_body_drop_names(context.actor_bytes, name, variant);
            const i16 kind = std::bit_cast<i16>(read_u16(context.actor_bytes, 0xB9C));
            FactoryObject object;
            if (kind == 0)
            {
                u32 prefix;
                std::memcpy(&prefix, name, 4);
                if (prefix == 0x5F646961)
                {
                    auto clone = clone_executable_resource(context.resources, name);
                    if (clone.bytes != nullptr)
                    {
                        std::memcpy(clone.bytes.get() + 8, &selected, 12);
                        std::memcpy(clone.bytes.get() + 0x14, &euler, 12);
                        const float scale = 1.f;
                        const u16 zero = 0;
                        std::memcpy(clone.bytes.get() + 0x20, &scale, 4);
                        std::memcpy(clone.bytes.get() + 0x2AA, &zero, 2);
                        std::memcpy(clone.bytes.get() + 0x2AC, &zero, 2);
                        char* output = reinterpret_cast<char*>(clone.bytes.get() + 0x5BA);
                        const char* source = variant;
                        char character;
                        do
                        {
                            character = *source++;
                            *output++ = character;
                        } while (character != '\0');
                        object = create_factory_object(context.factory, {clone.bytes.get(), clone.size});
                        clone.bytes.reset();
                    }
                }
                const std::array<u32, 9> message{0x4D, 3, std::bit_cast<u32>(position.x),
                    std::bit_cast<u32>(position.y), std::bit_cast<u32>(position.z),
                    std::bit_cast<u32>(euler.x), std::bit_cast<u32>(euler.y),
                    std::bit_cast<u32>(euler.z), 0};
                const u32 drop = context.keep(std::move(object));
                if (drop == 0)
                {
                    throw Error("0x00135080 messages a drop actor it could not create");
                }
                context.message(drop, message); // Drop vtable +0xC.
            }
            else
            {
                if (kind != 1 && kind != 2)
                {
                    return;
                }
                object = create_runtime_actor_at(name, &selected, &euler, 1.f,
                    context.resources, context.factory, context.default_position, context.default_euler);
                if (object.bytes == nullptr)
                {
                    return;
                }
                std::array<u32, 7> message{0x96, std::bit_cast<u32>(position.x),
                    std::bit_cast<u32>(position.y), std::bit_cast<u32>(position.z),
                    std::bit_cast<u32>(euler.x), std::bit_cast<u32>(euler.y), std::bit_cast<u32>(euler.z)};
                if (kind == 2)
                {
                    message[5] = 0;
                }
                context.message(context.keep(std::move(object)), message); // Drop vtable +0xC.
            }
        }
    }
}
}

