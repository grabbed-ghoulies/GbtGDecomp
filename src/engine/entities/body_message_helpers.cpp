#include "body_message_helpers.h"
#include <cstring>

namespace gbtg
{
// Xbox 0x0001EF40
bool runtime_body_bit_matches(const EntityBitBlock& flags, u32 index, u32 expected)
{
    return expected == u32((flags.bytes[index >> 3] & (u8(1) << (index & 7))) != 0);
}

// Xbox 0x0001EFF0
void assign_runtime_body_bit(EntityBitBlock& flags, u32 index, u32 value)
{
    if (value != 0)
    {
        flags.bytes[index >> 3] |= u8(u8(1) << (index & 7));
        return;
    }
    flags.bytes[index >> 3] &= u8(~(u8(1) << (index & 7)));
}

// Xbox 0x0001EF10
bool runtime_body_bit_set(const EntityBitBlock& flags, u32 index)
{
    return (flags.bytes[index >> 3] & u8(1u << (index & 7))) != 0;
}

// Xbox 0x0001EFD0
void set_runtime_body_bit(EntityBitBlock& flags, u32 index)
{
    flags.bytes[index >> 3] |= u8(1u << (index & 7));
}

// Xbox 0x0001F030
void clear_runtime_body_bit(EntityBitBlock& flags, u32 index)
{
    flags.bytes[index >> 3] &= u8(~(1u << (index & 7)));
}

// Xbox 0x0001AEB0
void clear_runtime_body_message_receivers(std::span<u8> body, u32 address)
{
    const u32 zero = 0;
    if (read_u32(body, 0x10) == address)
    {
        std::memcpy(body.data() + 0x10, &zero, 4);
    }
    if (read_u32(body, 0x14) == address)
    {
        std::memcpy(body.data() + 0x14, &zero, 4);
    }
}

// Xbox 0x00024340
void clear_runtime_body_damage_sources(std::span<u8> body, u32 address)
{
    const u32 zero = 0;
    if (read_u32(body, 0x274) == address)
    {
        std::memcpy(body.data() + 0x274, &zero, 4);
    }
    if (read_u32(body, 0x270) == address)
    {
        std::memcpy(body.data() + 0x270, &zero, 4);
    }
}

// Xbox 0x00027B20
void clear_runtime_body_timer_recipient(std::span<u8> body, u32 address)
{
    const u32 zero = 0;
    if (read_u32(body, 0x760) == address)
    {
        std::memcpy(body.data() + 0x760, &zero, 4);
        std::memcpy(body.data() + 0x758, &zero, 4);
    }
    if (read_u32(body, 0x7F0) == address)
    {
        std::memcpy(body.data() + 0x7F0, &zero, 4);
        std::memcpy(body.data() + 0x7E8, &zero, 4);
    }
}

// Xbox 0x0001B570
RuntimeBodyAttachedActor* detach_runtime_body_secondary(std::span<u8> body,
    RuntimeBodyAttachedActor*& secondary, RuntimeBodyAnimationReceiver*& receiver)
{
    RuntimeBodyAttachedActor* attached = secondary;
    const u32 address = read_u32(body, 8);
    const u32 zero = 0;
    receiver = nullptr;
    std::memcpy(body.data() + 0x14, &zero, 4);
    std::array<u32, 3> message{0x9D, address, 0};
    attached->message(message);
    detach_actor_bind(*attached->actor);
    secondary = nullptr;
    receiver = nullptr;
    std::memcpy(body.data() + 0x4C, &zero, 4);
    std::memcpy(body.data() + 0x14, &zero, 4);
    message = {0x9C, address, 0};
    attached->message(message);
    return attached;
}

// Xbox 0x0001B100
void clear_runtime_body_actor_reference(std::span<u8> body, u32 address,
    RuntimeBodyActorAttachment& primary, RuntimeBodyAttachedActor*& secondary,
    RuntimeBodyAnimationReceiver*& receiver)
{
    const u32 zero = 0;
    if (address == read_u32(body, 8))
    {
        if (primary.attached != nullptr)
        {
            detach_runtime_body_actor(body, primary);
        }
        if (secondary != nullptr)
        {
            detach_runtime_body_secondary(body, secondary, receiver);
        }
    }
    else
    {
        if (read_u32(body, 0x48) == address)
        {
            primary.attached = nullptr;
            std::memcpy(body.data() + 0x48, &zero, 4);
        }
        if (read_u32(body, 0x4C) == address)
        {
            secondary = nullptr;
            std::memcpy(body.data() + 0x4C, &zero, 4);
        }
    }
    if (read_u32(body, 0x50) == address)
    {
        std::memcpy(body.data() + 0x50, &zero, 4);
        std::memcpy(body.data() + 0x54, &zero, 4);
    }
}

// Xbox 0x000275E0
void clear_runtime_body_action_children(std::span<u8> body, RuntimeBodyActionSelection& context)
{
    const u32 zero = 0;
    if (context.first.bytes != nullptr)
    {
        release_factory_object(context.child.factory, context.first);
        std::memcpy(body.data() + 0x314, &zero, 4);
    }
    if (context.second.bytes != nullptr)
    {
        release_factory_object(context.child.factory, context.second);
        std::memcpy(body.data() + 0x744, &zero, 4);
    }
    context.row = nullptr;
    std::memcpy(body.data() + 0x740, &zero, 4);
    const u16 key = 0;
    std::memcpy(body.data() + 0x748, &key, 2);
}

// Xbox 0x000277E0
void reselect_runtime_body_action(std::span<u8> body, RuntimeBodyActionMessages& context)
{
    const u16 key = context.attachment.body_action_key();
    if (key != 0 && context.actions.table != nullptr)
    {
        u32 parameters;
        const u8* row = find_runtime_script_table_row(context.actions.table, key, parameters);
        if (row != nullptr)
        {
            select_runtime_body_action_row(body, row, nullptr, context.actions);
        }
    }
}

// Xbox 0x0001F750
bool runtime_body_latches_block(std::span<const u8> body, const Actor& actor, u32 mask)
{
    if (read_u16(body, 0x94) == 0 || !(actor.animation_pause > 0.f))
    {
        for (u32 index = 0; index < 6; ++index)
        {
            if (((mask >> index) & 1) != 0 && body[0x260 + index] != 0)
            {
                return true;
            }
        }
    }
    return false;
}

// Xbox 0x0001D4C0
bool runtime_body_can_receive_push(std::span<const u8> actor,
    std::span<const u8> source, i32 mode)
{
    const bool receiver = read_u32(actor, 0xCE0) != 0
        && (read_u16(actor, 0xCDC) == 2 || read_u32(actor, 0x93C) != 0);
    const bool sender = read_u32(source, 0xCE0) != 0
        && (read_u16(source, 0xCDC) == 2 || read_u32(source, 0x93C) != 0);
    return mode == 1 && receiver && !sender;
}

// Xbox 0x00024BA0
void receive_runtime_body_push(std::span<const u8> body, Actor& actor,
    std::span<const u8> actor_bytes, std::span<const u8> source,
    const u8* parameters, const Vec3& push)
{
    if (body[0x6A] != 0)
    {
        const u8 mode = parameters != nullptr ? parameters[6] : 1;
        if (runtime_body_can_receive_push(actor_bytes, source, mode))
        {
            const float y = push.y;
            const float z = push.z;
            actor.anchor.x = push.x + actor.anchor.x;
            actor.anchor.y = y + actor.anchor.y;
            actor.anchor.z = z + actor.anchor.z;
        }
    }
}

// Xbox 0x00024B20
bool notify_runtime_body_action_key(std::span<u8> body, u16 key, RuntimeBodyActionMessages& context)
{
    const std::array<u32, 2> message{0x61, key};
    context.actor_message(message);
    if (key != 0 && context.actions.table != nullptr)
    {
        u32 parameters;
        const u8* row = find_runtime_script_table_row(context.actions.table, key, parameters);
        if (row != nullptr)
        {
            select_runtime_body_action_row(body, row, nullptr, context.actions);
            return true;
        }
    }
    return false;
}

// Xbox 0x00020920
void reset_runtime_body_action(std::span<u8> body, std::span<const u8> settings,
    RuntimeBodyActionMessages& context)
{
    if (read_u16(body, 0x94) != 0)
    {
        const u32 second = read_u32(settings, 0x1AC);
        const u32 first = read_u32(settings, 0x1A8);
        if (first != 0)
        {
            select_runtime_body_animation(context.actions.names.at(first), context.actions);
        }
        if (second != 0)
        {
            select_runtime_body_animation(context.actions.names.at(second), context.actions);
        }
        release_runtime_player_attachment(0, 0, context.attachment);
        const u16 key = context.attachment.body_action_key();
        notify_runtime_body_action_key(body, key, context);
    }
}

// Xbox 0x0013AD60
void dispatch_runtime_body_position_event(const Vec3& position, i32 id, RuntimeBodyActionMessages& context)
{
    g_actor_creation_options.flags = 0;
    g_actor_creation_options.position = position;
    g_actor_creation_options.flags = 1;
    if (context.events.actor_address == 0xFFFFFF74u)
    {
        return;
    }
    dispatch_actor_attachment_records(context.event_record, id, nullptr,
        &g_actor_creation_options, context.events, 0);
}

// Xbox 0x00020980
void request_runtime_body_action_6e(std::span<u8> body, const Vec3& position, RuntimeBodyActionMessages& context)
{
    dispatch_runtime_body_position_event(position, 4, context);
    if (!runtime_body_latches_block(body, context.events.actor, 0x3F)
        && context.actions.table != nullptr)
    {
        u32 parameters;
        const u8* row = find_runtime_script_table_row(context.actions.table, 0x6E, parameters);
        if (row != nullptr)
        {
            select_runtime_body_action_row(body, row, nullptr, context.actions);
        }
    }
}

// Xbox 0x000209E0
void request_runtime_body_action_7b(std::span<u8> body, std::span<u8> message,
    const EntityBitBlock& flags, RuntimeBodyActionMessages& context)
{
    const Vec3 position{read_f32(message, 0xC), read_f32(message, 0x10), read_f32(message, 0x14)};
    dispatch_runtime_body_position_event(position, 4, context);
    if (runtime_body_latches_block(body, context.events.actor, 0x3F) || (flags.bytes[4] & 2) != 0)
    {
        return;
    }
    if (context.actions.table != nullptr)
    {
        u32 parameters;
        const u8* row = find_runtime_script_table_row(context.actions.table, 0x7B, parameters);
        if (row != nullptr)
        {
            select_runtime_body_action_row(body, row, nullptr, context.actions);
            const u32 reply = 1;
            std::memcpy(message.data() + 4, &reply, 4);
            return;
        }
    }
    const u32 reply = 0;
    std::memcpy(message.data() + 4, &reply, 4);
}

// Xbox 0x00075960
bool notify_runtime_player_collision_action(Vec3& proposed, const Vec3& start,
    Vec3& reference, Vec3& upper, u8* contact, RuntimePlayerCollisionAction*& action)
{
    RuntimePlayerCollisionReply message{0x62, 0, 0, &proposed, &start, &reference, &upper, contact};
    if (action != nullptr)
    {
        action->message(message);
        if (message.accepted != 0 && message.handled != 0)
        {
            return true;
        }
    }
    return false;
}
}
