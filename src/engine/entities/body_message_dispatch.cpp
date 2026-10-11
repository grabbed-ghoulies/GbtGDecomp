#include "body_message_dispatch.h"
#include <cstring>

namespace gbtg
{
// Xbox 0x000253B0
void dispatch_runtime_parent_body_message(std::span<u8> body, std::span<u8> message,
    const RuntimeBodyMessageViews& views, RuntimeBodyMessageDispatch& context)
{
    auto& actions = context.actions.actions;
    if (actions.second.bytes != nullptr)
    {
        context.children.at(actions.second.bytes.get())->message(message);
    }
    if (actions.first.bytes != nullptr)
    {
        context.children.at(actions.first.bytes.get())->message(message);
    }
    const u32 actor_address = read_u32(body, 8);
    switch (read_u32(message, 0))
    {
    case 6:
        request_runtime_body_contact_override(context.collision, views.contact, context.players);
        return;
    case 7:
        dispatch_runtime_body_animation(body, message, views.animation_command, context.animation);
        return;
    case 9:
        clear_runtime_body_action_children(body, actions);
        context.actor.anchor = context.default_position;
        std::memcpy(context.actor_bytes.data() + 0x3C, &context.default_position, 12);
        std::memcpy(body.data() + 0x318, &context.default_position, 12);
        increment_runtime_body_latches(body, context.actor, context.latches);
        return;
    case 0xA:
        if (context.actor.parked_gate < 1)
        {
            reselect_runtime_body_action(body, context.actions);
        }
        decrement_runtime_body_latches(body, context.actor, context.latches);
        return;
    case 0x14:
        clear_runtime_body_timer_recipient(body, read_u32(message, 4));
        // Keep native pointer views aligned with the retail words cleared by 0x00027B20.
        context.first_timer = read_u32(body, 0x760) == 0 ? nullptr : context.first_timer;
        context.second_timer = read_u32(body, 0x7F0) == 0 ? nullptr : context.second_timer;
        return;
    case 0x23:
        capture_runtime_body_spatial_pose(body, context.shape_scratch, context.spatial);
        return;
    case 0x47:
        if (context.collision != nullptr)
        {
            remove_runtime_body_contact_actor(*context.collision, *views.referenced_actor);
            return;
        }
        break;
    case 0x4B:
    {
        Vec3 position;
        Vec3 euler;
        std::memcpy(&position, context.actor_bytes.data() + 0xCC0, 12);
        std::memcpy(&euler, context.actor_bytes.data() + 0xCCC, 12);
        set_runtime_actor_body_position(context.actor, position);
        set_runtime_actor_body_euler(context.actor, euler);
        body[0x6B] = 0;
        body[0x6A] = 0;
        const u16 health = read_u16(body, 0x96);
        std::memcpy(body.data() + 0x94, &health, 2);
        clear_runtime_body_bit(context.animation.flags, 0x1B);
        select_runtime_body_action(body, read_u16(body, 0x74A), actions);
        return;
    }
    case 0x4E:
    {
        const u32 value = read_u32(message, 4);
        std::memcpy(body.data() + 0x70, &value, 4);
        return;
    }
    case 0x4F:
    {
        body[0x87E] = message[4];
        const u16 value = 2;
        std::memcpy(body.data() + 0x880, &value, 2);
        return;
    }
    case 0x63:
    {
        Vec3 position;
        std::memcpy(&position, body.data() + 0x18, 12);
        const Vec3 offset{0.f, 0.f, 0.f};
        EntityCylinderBound query;
        query.kind = 4;
        query.size = 0x20;
        query.flag = 1;
        query.x = position.x;
        query.y = position.y;
        query.z = position.z;
        query.height = context.actor.radius_y * read_f32(body, 0x884);
        query.radius = context.actor.radius_xz * read_f32(body, 0x888);
        Vec3 push;
        if (entity_cylinder_collision_push(query, *views.cylinder, offset, push) != 0)
        {
            position.x = push.x + position.x;
            position.y = push.y + position.y;
            position.z = push.z + position.z;
            std::memcpy(body.data() + 0x18, &position, 12);
            return;
        }
        break;
    }
    case 0x7B:
        request_runtime_body_shape_override(context.collision, views.contact, context.players);
        break;
    case 0x7C:
    {
        const u32 address = read_u32(message, 4);
        clear_runtime_body_message_receivers(body, address);
        // These are pointer-view updates, not additional retail message gates.
        actions.first_receiver = read_u32(body, 0x10) == 0 ? nullptr : actions.first_receiver;
        actions.second_receiver = read_u32(body, 0x14) == 0 ? nullptr : actions.second_receiver;
        auto& attachment = context.actions.attachment;
        clear_runtime_body_actor_reference(body, address, attachment.ownership, attachment.secondary, actions.second_receiver);
        if (context.collision != nullptr)
        {
            remove_runtime_body_contact_actor(*context.collision, *views.referenced_actor);
        }
        clear_runtime_body_damage_sources(body, address);
        if (address == read_u32(body, 0x718))
        {
            const u32 zero = 0;
            std::memcpy(body.data() + 0x718, &zero, 4);
        }
        if (address == actor_address)
        {
            clear_runtime_body_action_children(body, actions);
            return;
        }
        break;
    }
    }
}
}
