#include "player_body_messages.h"
#include "actor_transform.h"
#include <cstring>

namespace gbtg
{
// Xbox 0x00074080
void dispatch_runtime_player_body_message(std::span<u8>& body, std::span<u8> message,
    const RuntimePlayerBodyMessageViews& views, RuntimePlayerBodyMessages& context)
{
    const u32 actor_address = read_u32(body, 8);
    auto& parent = context.parent;
    auto& processor = context.damage.processor;
    switch (read_u32(message, 0))
    {
    case 0xF:
        request_runtime_body_timer_contact(body, message, *views.source,
            views.source_body, views.bound, context.damage);
        return;
    case 0x15:
        dispatch_runtime_parent_body_message(body, message, views.parent, parent);
        receive_runtime_body_source_contact(body, message, *views.source_contact);
        return;
    case 0x16:
        dispatch_runtime_parent_body_message(body, message, views.parent, parent);
        receive_runtime_body_contact_health_hit(body, message, views.source_contact->contacts, context.damage);
        return;
    case 0x17:
        dispatch_runtime_parent_body_message(body, message, views.parent, parent);
        receive_runtime_body_contact_knockdown(body, message, views.source_contact->contacts, context.damage);
        return;
    case 0x18:
        dispatch_runtime_parent_body_message(body, message, views.parent, parent);
        receive_runtime_body_projectile_contact(body, message, views.source_contact->contacts, context.damage);
        return;
    case 0x19:
        dispatch_runtime_parent_body_message(body, message, views.parent, parent);
        receive_runtime_body_moving_contact(body, message, views.source_contact->contacts, context.damage);
        return;
    case 0x1B:
        dispatch_runtime_parent_body_message(body, message, views.parent, parent);
        receive_runtime_body_impact(body, message, views.source_contact->contacts,
            context.tag_rows, context.tag_names, context.damage);
        return;
    case 0x1C:
        dispatch_runtime_parent_body_message(body, message, views.parent, parent);
        receive_runtime_body_health_notice(body, message, context.first_health_names,
            context.second_health_names, context.damage);
        return;
    case 0x1D:
        dispatch_runtime_parent_body_message(body, message, views.parent, parent);
        receive_runtime_body_owned_contact(body, message, *views.source_contact);
        return;
    case 0x23:
        dispatch_runtime_parent_body_message(body, message, views.parent, parent);
        if (read_u32(body, 0x258) == 0)
        {
            separate_runtime_player_body_contacts(body, parent.actor, processor.flags,
                context.mask, context.floor, parent.default_position);
        }
        register_actor_wide_list(context.registration);
        return;
    case 0x27:
        if (runtime_player_body_can_select_action(body, parent.actor_bytes,
            context.goals, processor.flags, context.address_zero))
        {
            const u16 key = parent.actions.attachment.body_action_key();
            if (key != read_u16(body, 0x748))
            {
                select_runtime_body_action(body, key, parent.actions.actions);
            }
        }
        return;
    case 0x2A:
    {
        dispatch_runtime_parent_body_message(body, message, views.parent, parent);
        const Vec3 position{read_f32(message, 8), read_f32(message, 0xC), read_f32(message, 0x10)};
        receive_runtime_body_direct_hit(body, *views.source, position, context.damage);
        return;
    }
    case 0x2F:
        request_runtime_body_lethal_action(body, parent.actions.actions);
        return;
    case 0x33:
    {
        const Vec3 position = actor_world_position(parent.actor);
        (void)position;
        finish_runtime_body_death(0, context.scene_flags, context.restart_requested, processor.death);
        return;
    }
    case 0x3C:
        request_runtime_body_action_7b(body, message, processor.flags, parent.actions);
        return;
    case 0x43:
        if (read_u32(message, 8) == actor_address)
        {
            clear_runtime_body_bit(processor.flags, 0x28);
        }
        dispatch_runtime_parent_body_message(body, message, views.parent, parent);
        return;
    case 0x44:
        dispatch_runtime_parent_body_message(body, message, views.parent, parent);
        if (read_u32(message, 8) == actor_address)
        {
            reset_runtime_body_action(body, context.settings, parent.actions);
            set_runtime_body_bit(processor.flags, 0x28);
        }
        return;
    case 0x45:
    {
        const Vec3 normal{read_f32(message, 0x10), read_f32(message, 0x14), read_f32(message, 0x18)};
        const Vec3 position{read_f32(message, 0x1C), read_f32(message, 0x20), read_f32(message, 0x24)};
        u32 handled;
        const u32 result = receive_runtime_body_collision_hit(body, *views.source,
            views.source_body, parent.actor_bytes, normal, position, message[0x29],
            u8(read_u16(message, 0x2A)), handled, context.damage);
        std::memcpy(message.data() + 8, &handled, 4);
        std::memcpy(message.data() + 4, &result, 4);
        return;
    }
    case 0x4A:
        if (read_u32(body, 0x258) == 0)
        {
            separate_runtime_player_body_contacts(body, parent.actor, processor.flags,
                context.mask, context.floor, parent.default_position);
        }
        return;
    case 0x4C:
    {
        const Vec3 position{read_f32(message, 8), read_f32(message, 0xC), read_f32(message, 0x10)};
        request_runtime_body_action_6e(body, position, parent.actions);
        return;
    }
    case 0x60:
    {
        const Vec3 push{read_f32(message, 8), read_f32(message, 0xC), read_f32(message, 0x10)};
        receive_runtime_body_push(body, parent.actor, parent.actor_bytes,
            views.source->bytes, context.hit.data(), push);
        return;
    }
    case 0x9E:
    {
        const Vec3 movement{read_f32(message, 4), read_f32(message, 8), read_f32(message, 0xC)};
        add_runtime_player_body_movement(body, movement, parent.actor, context.clock);
        return;
    }
    default:
        dispatch_runtime_parent_body_message(body, message, views.parent, parent);
        return;
    }
}
}
