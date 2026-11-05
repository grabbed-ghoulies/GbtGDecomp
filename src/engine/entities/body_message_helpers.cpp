#include "body_message_helpers.h"
#include <cstddef>
#include "body_fields.h"
#include "player_fields.h"
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
    if (body_record(body).first_receiver == address)
    {
        body_record(body).first_receiver = zero;
    }
    if (body_record(body).second_receiver == address)
    {
        body_record(body).second_receiver = zero;
    }
}

// Xbox 0x00024340
void clear_runtime_body_damage_sources(std::span<u8> body, u32 address)
{
    const u32 zero = 0;
    if (body_record(body).unk_274 == address)
    {
        body_record(body).unk_274 = zero;
    }
    if (body_record(body).attacker == address)
    {
        body_record(body).attacker = zero;
    }
}

// Xbox 0x00027B20
void clear_runtime_body_timer_recipient(std::span<u8> body, u32 address)
{
    const u32 zero = 0;
    if (body_record(body).hit_timer == address)
    {
        body_record(body).hit_timer = zero;
        body_record(body).unk_758 = 0.f;
    }
    if (body_record(body).second_timer == address)
    {
        body_record(body).second_timer = zero;
        body_record(body).unk_7E8 = 0.f;
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
    body_record(body).second_receiver = zero;
    std::array<u32, 3> message{0x9D, address, 0};
    attached->message(message);
    detach_actor_bind(*attached->actor);
    secondary = nullptr;
    receiver = nullptr;
    body_record(body).unk_4C = zero;
    body_record(body).second_receiver = zero;
    message = {0x9C, address, 0};
    attached->message(message);
    return attached;
}

// Xbox 0x0001B6E0
void drop_runtime_body_secondary(std::span<u8> body, RuntimeBodyAttachedActor*& secondary,
    RuntimeBodyAnimationReceiver*& receiver)
{
    RuntimeBodyAttachedActor* held = secondary;
    if (held == nullptr)
    {
        return;
    }
    detach_runtime_body_secondary(body, secondary, receiver); // 0x0001B570
    const std::array<u32, 2> drop{0x97, 0};
    held->message(drop);
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
        if (body_record(body).attached == address)
        {
            primary.attached = nullptr;
            body_record(body).attached = zero;
        }
        if (body_record(body).unk_4C == address)
        {
            secondary = nullptr;
            body_record(body).unk_4C = zero;
        }
    }
    if (body_record(body).unk_50 == address)
    {
        body_record(body).unk_50 = zero;
        body_record(body).unk_54 = zero;
    }
}

// Xbox 0x000275E0
void clear_runtime_body_action_children(std::span<u8> body, RuntimeBodyActionSelection& context)
{
    const u32 zero = 0;
    if (context.first.bytes != nullptr)
    {
        release_factory_object(context.child.factory, context.first);
        body_record(body).unk_314 = zero;
    }
    if (context.second.bytes != nullptr)
    {
        release_factory_object(context.child.factory, context.second);
        body_record(body).unk_744 = zero;
    }
    context.row = nullptr;
    body_record(body).unk_740 = zero;
    const u16 key = 0;
    body_record(body).action_key = key;
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
    if (body_record(body).energy == 0 || !(actor.animation_pause > 0.f))
    {
        for (u32 index = 0; index < 6; ++index)
        {
            if (((mask >> index) & 1) != 0 && body[offsetof(BodyRecord, unk_260) + index] != 0)
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
    const bool receiver = player_record(actor).live != 0
        && (player_record(actor).mode == 2 || player_record(actor).case_link != 0);
    const bool sender = player_record(source).live != 0
        && (player_record(source).mode == 2 || player_record(source).case_link != 0);
    return mode == 1 && receiver && !sender;
}

// Xbox 0x00024BA0
void receive_runtime_body_push(std::span<const u8> body, Actor& actor,
    std::span<const u8> actor_bytes, std::span<const u8> source,
    const u8* parameters, const Vec3& push)
{
    if (body_record(body).unk_6A != 0)
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
    if (body_record(body).energy != 0)
    {
        const u32 second = read_u32(settings, 0x1AC);
        const u32 first = read_u32(settings, 0x1A8);
        const auto name = [&](u32 address)
        {
            return context.image_string ? context.image_string(address) : context.actions.names.at(address);
        };
        if (first != 0)
        {
            select_runtime_body_animation(name(first), context.actions);
        }
        if (second != 0)
        {
            select_runtime_body_animation(name(second), context.actions);
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
