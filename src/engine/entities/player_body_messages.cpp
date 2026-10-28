#include "player_body_messages.h"
#include "actor_transform.h"
#include "player_mind_events.h"
#include <bit>
#include <cstring>

namespace gbtg
{
// Xbox 0x00075C60
void begin_runtime_player_body_control(std::span<u8> body, std::span<const u8> message,
    RuntimePlayerBodyControlMessages& context)
{
    if (read_u32(message, 4) != read_u32(body, 8))
    {
        return;
    }
    switch (read_u32(message, 8))
    {
    case 3:
    {
        const u16 health = read_u16(body, 0x94);
        const u16 maximum = u16(read_u16(body, 0x96) + read_u32(message, 0xC));
        std::memcpy(body.data() + 0x94, &maximum, 2);
        std::memcpy(body.data() + 0x96, &maximum, 2);
        const u16 updated = u16(health + read_u16(message, 0xC));
        std::memcpy(body.data() + 0x94, &updated, 2);
        if (read_u32(body, 0x94C) != 0)
        {
            const u32 saved = read_u32(body, 0x948) + read_u32(message, 0xC);
            std::memcpy(body.data() + 0x948, &saved, 4);
        }
        return;
    }
    case 4:
    {
        const u16 health = read_u16(body, 0x94);
        const u32 zero = 0;
        if (health != 0)
        {
            if (read_u32(body, 0x94C) == 0)
            {
                const u32 saved = health;
                const u32 one = 1;
                std::memcpy(body.data() + 0x948, &saved, 4);
                std::memcpy(body.data() + 0x94C, &one, 4);
            }
            const u16 one = 1;
            std::memcpy(body.data() + 0x94, &one, 2);
            return;
        }
        std::memcpy(body.data() + 0x948, &zero, 4);
        return;
    }
    case 8:
        if (read_u32(message, 0x10) == 0)
        {
            context.events.command_attachments = body.subspan(0x924, 36);
            request_runtime_player_attachment_events(context.resource.resource.data(),
                context.events, 0x2E, context.body_address + 0x924);
        }
        return;
    case 9:
        if (read_u32(message, 0x10) == 0)
        {
            ++context.hidden;
            clear_actor_model_anchors(context.anchors);
            dispatch_actor_resource_callout(context.resource, 0x38, nullptr, context.events);
            if (context.attachment != nullptr)
            {
                suspend_actor_live_attachment(*context.attachment, true);
            }
        }
        return;
    case 10:
        set_runtime_body_bit(context.flags, 7);
        return;
    case 0xE:
        context.actor.animation_rate = 0.5f;
        return;
    case 0xF:
        context.actor.animation_rate = 1.5f;
        return;
    default:
        return;
    }
}

// Xbox 0x00075DD0
void end_runtime_player_body_control(std::span<u8> body, std::span<const u8> message,
    RuntimePlayerBodyControlMessages& context)
{
    if (read_u32(message, 4) != read_u32(body, 8))
    {
        return;
    }
    switch (read_u32(message, 8))
    {
    case 4:
    {
        if (read_u16(body, 0x94) != 0 && read_u32(body, 0x948) != 0)
        {
            const u16 health = u16(read_u32(body, 0x948));
            std::memcpy(body.data() + 0x94, &health, 2);
        }
        const u32 zero = 0;
        std::memcpy(body.data() + 0x948, &zero, 4);
        std::memcpy(body.data() + 0x94C, &zero, 4);
        return;
    }
    case 8:
        clear_runtime_player_attachment_events(body.subspan(0x924, 36), context.lists);
        return;
    case 9:
        --context.hidden;
        dispatch_actor_resource_callout(context.resource, 0x39, nullptr, context.events);
        register_actor_model_anchors(context.actor, context.anchors);
        if (context.attachment != nullptr)
        {
            --context.attachment->retained_18;
        }
        return;
    case 10:
        clear_runtime_body_bit(context.flags, 7);
        return;
    case 0xE:
    case 0xF:
        context.actor.animation_rate = 1.f;
        return;
    default:
        return;
    }
}

// Xbox 0x00075B60
void dispatch_runtime_player_body(std::span<u8>& body, std::span<u8> message,
    const RuntimePlayerBodyMessageViews& views, RuntimePlayerBodyMessages& context,
    RuntimePlayerBodyControlMessages& controls)
{
    if (read_u32(message, 0) == 0x86)
    {
        begin_runtime_player_body_control(body, message, controls);
        return;
    }
    if (read_u32(message, 0) == 0x87)
    {
        end_runtime_player_body_control(body, message, controls);
        return;
    }
    dispatch_runtime_player_body_message(body, message, views, context);
}

// Xbox 0x00075FC0
void dispatch_runtime_vampire_body_message(std::span<u8>& body, std::span<u8> message,
    const RuntimePlayerBodyMessageViews& views, RuntimePlayerBodyMessages& context, const EntityBitBlock& bits,
    std::span<const u8> actor, u32 body_address)
{
    if (read_u32(message, 0) == 0x19 && views.impact == nullptr)
    {
        throw Error("vampire body message 0x19 has no contact views");
    }
    if (read_u32(message, 0) != 0x19 || views.impact->source.bytes[0xDAC] != 0xC) // Message +0x1C's +0xDAC.
    {
        dispatch_runtime_player_body_message(body, message, views, context);
        return;
    }
    if ((bits.bytes[1] & 1) == 0)
    {
        // 0x00105F90 writes the stack slot that held the body argument, and leaves it for a zero direction.
        float angle = std::bit_cast<float>(body_address);
        Vec3 direction;
        std::memcpy(&direction, message.data() + 0x20, 12);
        runtime_direction_angle(direction, angle);
        constexpr float pi = std::bit_cast<float>(0x40490FDBu); // DAT_0032B148
        constexpr float minus_pi = std::bit_cast<float>(0xC0490FDBu); // DAT_0032B29C
        constexpr float two_pi = std::bit_cast<float>(0x40C90FDBu); // DAT_0032B298
        float offset = angle - pi - read_f32(actor, 0x18);
        while (offset < minus_pi)
        {
            offset = offset + two_pi;
        }
        if (offset < std::bit_cast<float>(0x3FA78D36u) && offset > std::bit_cast<float>(0xBEB2B8C3u))
        {
            // DAT_0032B6E8 (75 degrees) and DAT_0032B6E4 (-20 degrees).
            dispatch_runtime_parent_body_message(body, message, views.parent, context.parent);
            throw Error("vampire body message 0x19 reaches the untranslated front hit 0x00022FB0");
        }
    }
    const u32 one = 1;
    const u32 zero = 0;
    std::memcpy(message.data() + 0x10, &one, 4);
    std::memcpy(message.data() + 0x14, &zero, 4);
    std::memcpy(message.data() + 0x18, &one, 4);
}

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
        if (views.impact == nullptr)
        {
            throw Error("body message 0x19 has no contact views");
        }
        receive_runtime_body_moving_contact(body, message, *views.impact, context.damage);
        return;
    case 0x1B:
        dispatch_runtime_parent_body_message(body, message, views.parent, parent);
        if (views.impact == nullptr)
        {
            throw Error("body message 0x1B has no contact views");
        }
        receive_runtime_body_impact(body, message, *views.impact,
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
