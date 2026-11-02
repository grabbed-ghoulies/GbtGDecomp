#include "body_health.h"
#include "../audio/listener_settings.h"
#include "player_fields.h"
#include <cstring>

namespace gbtg
{
// Xbox 0x0001DB10
void initialize_runtime_body_health(std::span<u8> body, std::span<const u8> parameters,
    RuntimeBodyHealth& context)
{
    const u16 tag = get(context.actor, player_field::damage_class);
    float health;
    if (tag != 0 && context.curve[1] != context.curve[0])
    {
        const float low = float(context.curve[0]);
        const float high = float(context.curve[1]);
        const float value = float(context.tags[size_t(tag) * 0x87 - 0x47]);
        const float maximum = read_f32(context.curve, 8);
        const float minimum = read_f32(context.curve, 4);
        health = ((value - low) / (high - low)) * (maximum - minimum) + minimum;
        if (maximum > minimum)
        {
            if (health > maximum)
            {
                health = maximum;
            }
            else if (minimum > health)
            {
                health = minimum;
            }
        }
        else
        {
            if (maximum > health)
            {
                health = maximum;
            }
            else if (health > minimum)
            {
                health = minimum;
            }
        }
    }
    else
    {
        health = read_f32(context.curve, 8);
    }
    const u16 integer = u16(truncate_runtime_float(health));
    std::memcpy(body.data() + 0x94, &integer, 2);
    std::memcpy(body.data() + 0x96, &integer, 2);
    body[0xA4] = 0;
    body[0xA6] = parameters[0x1A8];
    const char* names[]
    {
        "aid_texture_ghoulies_scoreboard_energy_baddielowenergy",
        "aid_texture_ghoulies_scoreboard_energy_traitorenergy",
        "aid_texture_ghoulies_scoreboard_energy_traitoralt",
        "aid_texture_ghoulies_scoreboard_pauseicons_clock"
    };
    for (size_t i = 0; i < 4; ++i)
    {
        const auto texture = acquire_animation_resource(context.registry, names[i]);
        if (texture.data() == nullptr)
        {
            resource_error(context.error, 2);
        }
        context.textures[i] = texture;
    }
}

// Xbox 0x0001DCA0
void release_runtime_body_health(RuntimeBodyHealth& context)
{
    for (auto& texture : context.textures)
    {
        if (texture.data() != nullptr)
        {
            release_animation_resource(context.registry, texture);
            texture = {};
        }
    }
}

// Xbox 0x00027580
void release_runtime_body_actions(std::span<u8> body, FactoryObject& first,
    FactoryObject& second, std::span<const u8>& resource, ObjectFactoryRegistry& factory,
    AnimationResourceRegistry& registry)
{
    if (first.bytes != nullptr)
    {
        release_factory_object(factory, first);
        first = {};
    }
    if (second.bytes != nullptr)
    {
        release_factory_object(factory, second);
        second = {};
    }
    const u32 zero = 0;
    std::memcpy(body.data() + 0x740, &zero, 4);
    std::memcpy(body.data() + 0x748, &zero, 2);
    if (resource.data() != nullptr)
    {
        release_animation_resource(registry, resource);
        resource = {};
        std::memcpy(body.data() + 0x740, &zero, 4);
    }
}
}
