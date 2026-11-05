#include "body_message_dispatch.h"
#include <cstddef>
#include "body_fields.h"
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
        if (context.bind_children)
        {
            context.bind_children();
        }
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
        std::memcpy(body.data() + offsetof(BodyRecord, velocity.x), &context.default_position, 12);
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
        context.first_timer = body_record(body).hit_timer == 0 ? nullptr : context.first_timer;
        context.second_timer = body_record(body).second_timer == 0 ? nullptr : context.second_timer;
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
        body_record(body).unk_6B = 0;
        body_record(body).unk_6A = 0;
        const u16 health = body_record(body).max_energy;
        body_record(body).energy = health;
        clear_runtime_body_bit(context.animation.flags, 0x1B);
        select_runtime_body_action(body, body_record(body).default_action_key, actions);
        return;
    }
    case 0x4E:
    {
        const u32 value = read_u32(message, 4);
        std::memcpy(body.data() + offsetof(BodyRecord, unk_70), &value, 4);
        return;
    }
    case 0x4F:
    {
        body_record(body).surface_override = message[4];
        const u16 value = 2;
        body_record(body).surface_override_frames = value;
        return;
    }
    case 0x63:
    {
        Vec3 position;
        std::memcpy(&position, body.data() + offsetof(BodyRecord, position.x), 12);
        const Vec3 offset{0.f, 0.f, 0.f};
        EntityCylinderBound query;
        query.kind = 4;
        query.size = 0x20;
        query.flag = 1;
        query.x = position.x;
        query.y = position.y;
        query.z = position.z;
        query.height = context.actor.radius_y * body_record(body).height_scale;
        query.radius = context.actor.radius_xz * body_record(body).radius_scale;
        Vec3 push;
        if (entity_cylinder_collision_push(query, *views.cylinder, offset, push) != 0)
        {
            position.x = push.x + position.x;
            position.y = push.y + position.y;
            position.z = push.z + position.z;
            std::memcpy(body.data() + offsetof(BodyRecord, position.x), &position, 12);
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
        actions.first_receiver = body_record(body).first_receiver == 0 ? nullptr : actions.first_receiver;
        actions.second_receiver = body_record(body).second_receiver == 0 ? nullptr : actions.second_receiver;
        auto& attachment = context.actions.attachment;
        clear_runtime_body_actor_reference(body, address, attachment.ownership, attachment.secondary, actions.second_receiver);
        if (context.collision != nullptr)
        {
            remove_runtime_body_contact_actor(*context.collision, *views.referenced_actor);
        }
        clear_runtime_body_damage_sources(body, address);
        if (address == body_record(body).rider)
        {
            const u32 zero = 0;
            body_record(body).rider = zero;
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
