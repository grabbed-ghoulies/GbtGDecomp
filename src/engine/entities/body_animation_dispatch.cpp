#include "body_animation_dispatch.h"

namespace gbtg
{
// Xbox 0x0001A8F0
u32 dispatch_runtime_body_animation(std::span<u8> body, std::span<const u8> message,
    std::span<const u8> command, RuntimeBodyAnimationDispatch& context)
{
    auto& attachment = context.attachments.attachment;
    switch (read_u16(command, 0xC))
    {
    case 2:
        command_runtime_body_contact_shapes(body, context.actor, context.collision, command, read_u32(message, 0x10));
        return 0;
    case 6:
        command_runtime_body_step_sound(body, command, context.sounds);
        return 0;
    case 7:
        command_runtime_body_latch_mask(body, context.actor, context.latches, u8(read_u32(command, 0x10)), read_u32(message, 0x10));
        return 0;
    case 8:
        command_runtime_body_side_counters(body, read_u32(command, 0x10), read_u32(message, 0x10));
        return 0;
    case 9:
        command_runtime_body_target_push(context.actor, context.flags, context.target, command);
        return 0;
    case 0xA:
        command_runtime_body_facing_push(context.actor, context.flags, command);
        return 0;
    case 0xB:
        command_runtime_attached_contact_shapes(context.attached_shapes, command, read_u32(message, 0x10));
        return 0;
    case 0xD:
    case 0x3A:
        command_runtime_body_flag(context.flags, reinterpret_cast<const char*>(command.data() + 0x10), context.flag_names, read_u32(message, 0x10));
        return 0;
    case 0xE:
        command_runtime_body_floor_sound(body, context.sounds);
        return 0;
    case 0xF:
        command_runtime_body_impact_sound(body, context.sounds);
        return 0;
    case 0x10:
        context.created_objects.push_back(command_runtime_body_create_secondary(command, context.name_scratch, context.query_name, context.attachments));
        return 0;
    case 0x12:
        command_runtime_body_breath(context.actor, context.flags, context.animation, read_u32(message, 0x10), read_u32(message, 8), command, context.effect_flags, context.effects.effects);
        return 0;
    case 0x14:
        command_runtime_body_detach_actors(body, command, attachment.ownership, attachment.secondary, attachment.actions.second_receiver);
        return 0;
    case 0x15:
        context.created_objects.push_back(command_runtime_body_create_attachment(reinterpret_cast<const char*>(command.data() + 0x10), context.attachments));
        return 0;
    case 0x17:
        command_runtime_body_attachment_event(context.primary_callout);
        return 0;
    case 0x18:
        command_runtime_body_proximity(context.actor, command, context.proximity);
        return 0;
    case 0x19:
        command_runtime_body_attachment_list(body, context.body_address, command[0x10] != 0,
            context.attachment_record, context.attachment_events, context.attachment_lists);
        return 0;
    case 0x1A:
        command_runtime_body_teleport(body, context.actor, command, attachment.ownership.attached, attachment.secondary, attachment.actions.second_receiver);
        return 0;
    case 0x1B:
        command_runtime_body_release_actor(body, attachment.ownership);
        return 0;
    case 0x28:
        command_runtime_body_surface_effect(body, command, context.effects);
        return 0;
    case 0x2B:
        command_runtime_body_primary_callout(context.primary_callout, reinterpret_cast<const char*>(command.data() + 0x10), context.attachment_names);
        return 0;
    case 0x2C:
        command_runtime_body_secondary_callout(context.secondary_callout, reinterpret_cast<const char*>(command.data() + 0x10), context.attachment_names);
        return 0;
    case 0x2D:
        context.created_objects.push_back(command_runtime_body_projectile(body, command, context.projectiles));
        return 0;
    case 0x2E:
        command_runtime_body_random_timer(body, read_f32(command, 0x10), read_f32(command, 0x14), context.effects.effects.random_state);
        break;
    }
    return 0;
}
}
