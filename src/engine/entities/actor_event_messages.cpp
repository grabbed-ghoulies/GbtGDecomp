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
    const u16 kind = get(event, animation_event_field::kind);
    switch (kind)
    {
    case 4:
        if (message.phase != 0)
        {
            set_actor_shader_bits(actor, static_cast<i16>(get(event, shader_bits_event::slot)),
                get(event, shader_bits_event::mask));
            return;
        }
        clear_actor_shader_bits(actor, static_cast<i16>(get(event, shader_bits_event::slot)),
            get(event, shader_bits_event::mask));
        return;
    case 0xC:
    case 0x11:
    case 0x26:
    case 0x27:
        if (message.phase == 0 && actor_event_chance(context.effects.random_state, get(event, effect_event::chance)))
        {
            if (kind == 0x11)
            {
                g_actor_effect_binding.mode = 1;
                const u32 previous = std::bit_cast<u32>(g_actor_effect_binding.index);
                g_actor_effect_binding.index =
                    std::bit_cast<i32>((previous & 0xFFFF0000) | u16(get(event, effect_event::binding)));
            }
            else
            {
                g_actor_effect_binding.mode = kind == 0x27 ? 2 : 0;
                g_actor_effect_binding.index = static_cast<i32>(get(event, effect_event::binding));
            }
            g_actor_effect_binding.actor = &actor;
            const auto& options = actor_creation_options(actor, &g_actor_effect_binding,
                static_cast<i32>(get(event, effect_event::scaled)),
                    static_cast<i32>(get(event, effect_event::include_euler)));
            if (load_actor_effect_stream(field_data(event, effect_event::name), &options, context.effects) != 0)
            {
                retain_last_actor_effect(actor_effect_event_duration(static_cast<i32>(get(event, effect_event::timed)),
                    event, context.state.animation));
            }
            if (field_data(event, effect_event::sound)[0] != 0)
            {
                request_actor_sound(actor, static_cast<i32>(get(event, effect_event::sound_slot)),
                    field_data(event, effect_event::sound), context.effects.sounds);
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
        const float scale =
            get(event, camera_effect_event::scaled) == 0 ? actor.radius_xz : actor.scale_200 * actor.scale_x;
        ActorCameraEffect* effect =
            allocate_actor_camera_effect(static_cast<u8>(get(event, camera_effect_event::effect)));
        const float size = get(event, camera_effect_event::size) * scale;
        std::memcpy(effect->payload + 0x28, &size, 4);
        std::memcpy(effect->payload + 0x1C, field_data(event, camera_effect_event::unk_18), 4);
        if (get(event, camera_effect_event::positioned) != 0)
        {
            const Vec3 position = actor_world_position(actor);
            std::memcpy(effect->payload, &position, 12);
            effect->payload[0x7A] = 1;
            const float low = get(event, camera_effect_event::low) * scale;
            const float high = get(event, camera_effect_event::high) * scale;
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
        stop_actor_sound(actor, static_cast<i32>(get(event, stop_sound_event::slot)), context.effects.sounds);
        return;
    case 0x24:
        load_actor_effect_stream(field_data(event, effect_event::name), nullptr, context.effects);
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
            if (get(event, trail_event::sets_state) != 0)
            {
                previous = static_cast<u8>(actor_shader_state(actor, get(event, trail_event::state_slot)));
                set_actor_node_state(context.state, get(event, trail_event::state_slot),
                    get(event, trail_event::state_value), context.shader_states);
            }
            request_actor_video_rate_trail(actor, get(event, trail_event::rate), context.trail_timer, context.trails);
            if (get(event, trail_event::sets_state) != 0)
            {
                set_actor_node_state(context.state, get(event, trail_event::state_slot), previous,
                    context.shader_states);
                return;
            }
        }
        break;
    case 0x29:
        if (message.phase == 0)
        {
            const float scale =
                get(event, effect_event::binding) == 0 ? actor.radius_xz : actor.scale_200 * actor.scale_x;
            if (std::memcmp(field_data(event, effect_event::name), "aid_", 4) == 0)
            {
                const Vec3 position = actor_world_position(actor);
                load_actor_camera_shake(field_data(event, effect_event::name), position, scale,
                    context.effects.models.registry, context.effects.models.error);
                return;
            }
        }
        break;
    case 0x2A:
        if (std::memcmp(field_data(event, effect_event::name), "aid_", 4) == 0)
        {
            auto object = create_resource_object_with_data(context.scares.factory, context.scares.resources,
                field_data(event, effect_event::name), context.trails.actor_address);
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
        if (is_audio_bank_resource(field_data(event, voice_event::name)))
        {
            request_audio_event_voice(field_data(event, voice_event::name), 1, context.voices);
        }
        break;
    case 0x31:
        set_actor_node_state(context.state, get(event, node_state_event::slot), get(event, node_state_event::value),
            context.shader_states);
        return;
    default:
        break;
    }
}
}
