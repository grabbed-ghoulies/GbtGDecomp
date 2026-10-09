#include "actor_trail_requests.h"

#include <cstring>

namespace gbtg
{
namespace
{
template<class T>
void store(u8* parameters, size_t offset, T value)
{
    std::memcpy(parameters + offset, &value, sizeof(value));
}
}

// Xbox 0x000411A0
void initialize_actor_effect_parameters(u8* parameters, const char* default_name)
{
    store<u16>(parameters, 0, 0x334);
    store<u16>(parameters, 2, 0x3E);
    store<u16>(parameters, 4, 2);
    store<float>(parameters, 0x10, 0.f);
    store<float>(parameters, 0xC, 0.f);
    store<float>(parameters, 8, 0.f);
    store<float>(parameters, 0x1C, 0.f);
    store<float>(parameters, 0x18, 0.f);
    store<float>(parameters, 0x14, 0.f);
    store<float>(parameters, 0x20, 1.f);
    store<float>(parameters, 0x24, 1.f);
    std::strcpy(reinterpret_cast<char*>(parameters + 0xA8), default_name);
    std::strcpy(reinterpret_cast<char*>(parameters + 0x28), default_name);
    std::strcpy(reinterpret_cast<char*>(parameters + 0x1A8), default_name);
    std::strcpy(reinterpret_cast<char*>(parameters + 0x128), default_name);
    std::strcpy(reinterpret_cast<char*>(parameters + 0x228), default_name);
    store<u16>(parameters, 0x2A8, 0xFFFF);
    store<u32>(parameters, 0x2B0, 0);
    std::strcpy(reinterpret_cast<char*>(parameters + 0x2B4), default_name);
    store<u16>(parameters, 0x2AA, 0);
    store<u16>(parameters, 0x2AC, 0);
}

// Xbox 0x0013B030
bool request_actor_trail(Actor& actor, float scale, float& timer, i32 frequency,
    ActorTrailContext& context)
{
    const float delta = context.pause > 0.f ? 0.f : context.frame_delta * context.time_scale;
    const float rate = actor.animation_pause > 0.f ? 0.f : actor.animation_rate;
    timer = rate * delta + timer;
    const float interval = 1.f / float(frequency);
    bool created = false;
    while (timer - interval > 0.f)
    {
        timer -= interval;
        if (!created)
        {
            // COMISS 0,scale takes its carry branch for unordered values too.
            if (!(scale <= 0.f))
            {
                std::array<u8, 0x358> parameters;
                initialize_actor_effect_parameters(parameters.data(), context.default_name);
                store<float>(parameters.data(), 0x24, actor.scale_200);
                store<float>(parameters.data(), 0x344, actor.scale_x);
                store<float>(parameters.data(), 0x348, actor.radius_y);
                store<u16>(parameters.data(), 0, 0x354);
                store<u16>(parameters.data(), 2, 0x43);
                store<u16>(parameters.data(), 4, 0x10);
                store<u32>(parameters.data(), 0x338, context.actor_address);
                store<float>(parameters.data(), 0x33C, scale);
                store<u8>(parameters.data(), 0x340, 0x80);
                store<u32>(parameters.data(), 0x354, 0);
                store<u32>(parameters.data(), 0x350, 0);
                store<float>(parameters.data(), 0x34C, actor.radius_xz);
                auto object = create_factory_object(context.factory, parameters);
                created = object.bytes != nullptr;
                if (created)
                {
                    context.objects.push_back(std::move(object));
                }
            }
            else
            {
                created = false;
            }
        }
    }
    return created;
}

// Xbox 0x0013B1B0
void request_actor_video_rate_trail(Actor& actor, float scale, float& timer,
    ActorTrailContext& context)
{
    i32 frequency;
    if (context.pal == 0 || context.pal_override != 0)
    {
        frequency = 60;
    }
    else
    {
        frequency = 50;
    }
    request_actor_trail(actor, scale, timer, frequency / 2, context);
}
}
