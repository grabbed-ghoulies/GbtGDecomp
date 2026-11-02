#include "actor_trail_requests.h"
#include "gbtg/object_factory.h"

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
    const std::span<u8> block{parameters, actor_parameter_field::kSize};
    set(block, object_parameter_field::size, u16(actor_parameter_field::kSize));
    set(block, object_parameter_field::unk_2, 0x3E);
    set(block, object_parameter_field::type, 2);
    set(block, actor_parameter_field::position, Vec3{0.f, 0.f, 0.f});
    set(block, actor_parameter_field::euler, Vec3{0.f, 0.f, 0.f});
    set(block, actor_parameter_field::scale, 1.f);
    set(block, actor_parameter_field::model_scale, 1.f);
    std::strcpy(field_data(block, actor_parameter_field::animation), default_name);
    std::strcpy(field_data(block, actor_parameter_field::model), default_name);
    std::strcpy(field_data(block, actor_parameter_field::resource), default_name);
    std::strcpy(field_data(block, actor_parameter_field::animation_table), default_name);
    std::strcpy(field_data(block, actor_parameter_field::attachment), default_name);
    set(block, actor_parameter_field::attachment_bone, 0xFFFF);
    set(block, actor_parameter_field::secondary_mode, 0);
    std::strcpy(field_data(block, actor_parameter_field::secondary_model), default_name);
    set(block, actor_parameter_field::marker_id, 0);
    set(block, actor_parameter_field::next_marker, 0);
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
                // The stores after the PUSH of the record address at 0x0013B123 are 4 bytes further up the
                // stack: +0x334 the source actor, +0x338 the life, +0x33C the alpha, +0x340..+0x348 actor +0x30.
                store<float>(parameters.data(), 0x24, actor.scale_200);
                store<float>(parameters.data(), 0x340, actor.scale_x);
                store<float>(parameters.data(), 0x344, actor.radius_y);
                store<u16>(parameters.data(), 0, 0x354);
                store<u16>(parameters.data(), 2, 0x43);
                store<u16>(parameters.data(), 4, 0x10);
                store<u32>(parameters.data(), 0x334, context.actor_address);
                store<float>(parameters.data(), 0x338, scale);
                store<u8>(parameters.data(), 0x33C, 0x80);
                store<u32>(parameters.data(), 0x350, 0);
                store<u32>(parameters.data(), 0x34C, 0);
                store<float>(parameters.data(), 0x348, actor.radius_xz);
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
