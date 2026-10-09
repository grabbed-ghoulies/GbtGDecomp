#include "actor_event_messages.h"
#include "../audio/bank_lookup.h"
#include "../camera/actor_camera_effects.h"

#include <bit>
#include <cstring>

namespace gbtg
{
// Xbox 0x0003A380
void dispatch_actor_animation_event(const ActorAnimationMessage& message,
    ActorEventContext& context)
{
    Actor& actor = context.state.actor;
    const auto event = message.event;
    const u8* bytes = event.data();
    const u16 kind = read_u16(event, 0xC);
    switch (kind)
    {
    case 4:
        if (message.phase != 0)
        {
            set_actor_shader_bits(actor, static_cast<i16>(read_u16(event, 0x10)), read_u32(event, 0x14));
            return;
        }
        clear_actor_shader_bits(actor, static_cast<i16>(read_u16(event, 0x10)), read_u32(event, 0x14));
        return;
    case 0xC:
    case 0x11:
    case 0x26:
    case 0x27:
        if (message.phase == 0 && actor_event_chance(context.effects.random_state, read_f32(event, 0xA0)))
        {
            if (kind == 0x11)
            {
                g_actor_effect_binding.mode = 1;
                const u32 previous = std::bit_cast<u32>(g_actor_effect_binding.index);
                g_actor_effect_binding.index = std::bit_cast<i32>((previous & 0xFFFF0000) | read_u16(event, 0x90));
            }
            else
            {
                g_actor_effect_binding.mode = kind == 0x27 ? 2 : 0;
                g_actor_effect_binding.index = static_cast<i32>(read_u32(event, 0x90));
            }
            g_actor_effect_binding.actor = &actor;
            const auto& options = actor_creation_options(actor, &g_actor_effect_binding,
                static_cast<i32>(read_u32(event, 0x98)), static_cast<i32>(read_u32(event, 0x9C)));
            if (load_actor_effect_stream(reinterpret_cast<const char*>(bytes + 0x10), &options, context.effects) != 0)
            {
                retain_last_actor_effect(actor_effect_event_duration(static_cast<i32>(read_u32(event, 0x94)),
                    event, context.state.animation));
            }
            if (bytes[0xA8] != 0)
            {
                request_actor_sound(actor, static_cast<i32>(read_u32(event, 0xA4)),
                    reinterpret_cast<const char*>(bytes + 0xA8), context.effects.sounds);
                return;
            }
        }
        break;
    case 0x13:
        if (message.phase == 0)
        {
            request_actor_scare(actor, context.trails.actor_address, context.scare_intensity,
                context.scare_scale, 0, 0, context.scares);
            return;
        }
        break;
    case 0x16:
    {
        const float scale = read_u32(event, 0x28) == 0 ? actor.radius_xz : actor.scale_200 * actor.scale_x;
        ActorCameraEffect* effect = allocate_actor_camera_effect(static_cast<u8>(read_u32(event, 0x10)));
        const float size = read_f32(event, 0x14) * scale;
        std::memcpy(effect->payload + 0x28, &size, 4);
        std::memcpy(effect->payload + 0x1C, bytes + 0x18, 4);
        if (read_u32(event, 0x1C) != 0)
        {
            const Vec3 position = actor_world_position(actor);
            std::memcpy(effect->payload, &position, 12);
            effect->payload[0x7A] = 1;
            const float low = read_f32(event, 0x20) * scale;
            const float high = read_f32(event, 0x24) * scale;
            std::memcpy(effect->payload + 0xC, &low, 4);
            std::memcpy(effect->payload + 0x10, &high, 4);
            return;
        }
        break;
    }
    case 0x1C:
    case 0x1D:
    case 0x1E:
    case 0x20:
    case 0x21:
        request_actor_event_sound(event, actor, context.effects.random_state, context.effects.sounds);
        return;
    case 0x1F:
        stop_actor_sound(actor, static_cast<i32>(read_u32(event, 0x10)), context.effects.sounds);
        return;
    case 0x24:
        load_actor_effect_stream(reinterpret_cast<const char*>(bytes + 0x10), nullptr, context.effects);
        return;
    case 0x25:
        if (message.phase == 0)
        {
            context.trail_timer = 0.f;
            return;
        }
        if (message.phase == 2)
        {
            u8 previous;
            if (read_u32(event, 0x14) != 0)
            {
                previous = static_cast<u8>(actor_shader_state(actor, bytes[0x18]));
                set_actor_node_state(context.state, bytes[0x18], bytes[0x19], context.shader_states);
            }
            request_actor_video_rate_trail(actor, read_f32(event, 0x10), context.trail_timer, context.trails);
            if (read_u32(event, 0x14) != 0)
            {
                set_actor_node_state(context.state, bytes[0x18], previous, context.shader_states);
                return;
            }
        }
        break;
    case 0x29:
        if (message.phase == 0)
        {
            const float scale = read_u32(event, 0x90) == 0 ? actor.radius_xz : actor.scale_200 * actor.scale_x;
            if (read_u32(event, 0x10) == 0x5F646961)
            {
                const Vec3 position = actor_world_position(actor);
                load_actor_camera_shake(reinterpret_cast<const char*>(bytes + 0x10), position, scale,
                    context.effects.models.registry, context.effects.models.error);
                return;
            }
        }
        break;
    case 0x2A:
        if (read_u32(event, 0x10) == 0x5F646961)
        {
            auto object = create_resource_object_with_data(context.scares.factory, context.scares.resources,
                reinterpret_cast<const char*>(bytes + 0x10), context.trails.actor_address);
            if (object.bytes != nullptr)
            {
                context.scares.objects.push_back(std::move(object));
            }
            return;
        }
        break;
    case 0x2F:
        if (!actor_sound_is_playing(actor, 1, context.effects.sounds))
        {
            request_actor_event_sound(event, actor, context.effects.random_state, context.effects.sounds);
            return;
        }
        break;
    case 0x30:
        if (is_audio_bank_resource(reinterpret_cast<const char*>(bytes + 0x14)))
        {
            request_audio_event_voice(reinterpret_cast<const char*>(bytes + 0x14), 1, context.voices);
        }
        break;
    case 0x31:
        set_actor_node_state(context.state, bytes[0x10], bytes[0x14], context.shader_states);
        return;
    default:
        break;
    }
}
}
