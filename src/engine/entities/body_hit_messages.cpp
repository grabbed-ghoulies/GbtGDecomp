#include "body_hit_messages.h"
#include "actor_transform.h"
#include "segment_records.h"
#include "player_attributes.h"
#include "player_overlay.h"
#include "player_mind_events.h"
#include "gbtg/anim.h"
#include "body_fields.h"
#include <cstring>
#include <bit>

namespace gbtg
{
// Xbox 0x000202E0
u32 receive_runtime_body_collision_hit(std::span<u8> body,
    RuntimeBodyAttachedActor& source, std::span<const u8> source_body,
    std::span<const u8> actor_bytes, const Vec3& normal, const Vec3& position,
    u32 mode, u8 joint, u32& handled, RuntimeBodyDamageEntry& context)
{
    auto& processor = context.processor;
    handled = 0;
    if (runtime_body_latches_block(body, processor.actor, 3) ||
        (processor.flags.bytes[4] & 0x10) != 0)
    {
        return 0;
    }
    const i32 source_state = i32(read_u16(source_body, 0x162)) + 1;
    Vec3 movement;
    std::memcpy(&movement, source.bytes.data() + 0x3C, 12);
    Vec3 direction = movement;
    if (direction.x != 0.f || direction.y != 0.f || direction.z != 0.f)
    {
        normalize_vector_in_place(&direction.x);
    }
    const float dot = float((static_cast<long double>(direction.z) * normal.z +
        static_cast<long double>(direction.y) * normal.y) +
        static_cast<long double>(direction.x) * normal.x);
    // Retail spills the absolute value separately, but compares the original dot.
    const float absolute = animation_absolute(dot);
    (void)absolute;
    const bool knockdown = mode == 0 && !(0.5f > dot) && source_state <= 2 &&
        std::bit_cast<i32>(read_u32(source_body, 0x738)) >= std::bit_cast<i32>(read_u32(body, 0x738));
    std::array<u8, 0x68> damage{};
    if (knockdown)
    {
        damage[0] = 3;
        damage[1] = 0x37;
        damage[2] = 0x68;
        const u32 one = 1;
        const u32 two = 2;
        std::memcpy(damage.data() + 8, &one, 4);
        std::memcpy(damage.data() + 4, &two, 4);
    }
    else
    {
        damage[0] = 1;
        damage[1] = 0x32;
        damage[2] = 0x65;
        const u32 one = 1;
        std::memcpy(damage.data() + 4, &one, 4);
    }
    const bool controlled = read_u16(actor_bytes, 0xCDC) == 2 ||
        read_u32(actor_bytes, 0x93C) != 0;
    const u32 uncontrolled = !controlled;
    std::memcpy(damage.data() + 0x14, &uncontrolled, 4);
    damage[3] = joint;
    std::memcpy(damage.data() + 0x18, &position, 12);
    std::memcpy(damage.data() + 0x24, &normal, 12);
    std::memcpy(damage.data() + 0x34, &source.address, 4);
    std::memcpy(damage.data() + 0x38, &source.address, 4);
    std::memcpy(damage.data() + 0x3C, source_body.data() + 0x274, 4);
    std::memcpy(damage.data() + 0x40, &movement, 12);
    std::memcpy(damage.data() + 0x4C, &movement, 12);
    const u32 timer = read_u32(source_body, 0x760) == 0 ? 0 : read_u32(source_body, 0x754);
    std::memcpy(damage.data() + 0x58, &timer, 4);
    const u16 actor_class = read_u16(source.bytes, 0xCF4);
    std::memcpy(damage.data() + 0x64, &actor_class, 2);
    const u32 one = 1;
    std::memcpy(damage.data() + 0x60, &one, 4);
    context.contacts = nullptr;
    processor.first_source = &source;
    processor.second_source = &source;
    processor.second_source_live = read_u32(source.bytes, 0xCE0);
    std::array<u32, 4> result;
    receive_runtime_body_damage(body, damage, result, context);
    dispatch_runtime_body_damage_reply(damage, result, context);
    handled = result[1];
    return result[1];
}

// Xbox 0x00020A60
void reflect_runtime_body_timer_hit(std::span<const u8> body, RuntimeBodyAttachedActor& actor,
    std::span<u8> source_body, std::span<u8> damage,
    RuntimeBodyDamageEntry& source_context, RuntimeBodyDamageEntry& reply_context)
{
    const u32 actor_address = read_u32(body, 8);
    if (read_u32(body, 0x760) != 0 &&
        (read_u32(source_body, 0x760) == 0 || read_u32(source_body, 0x754) == 0))
    {
        float value = 0.f - read_f32(damage, 0x40);
        std::memcpy(damage.data() + 0x40, &value, 4);
        value = 0.f - read_f32(damage, 0x44);
        std::memcpy(damage.data() + 0x44, &value, 4);
        value = 0.f - read_f32(damage, 0x48);
        std::memcpy(damage.data() + 0x48, &value, 4);
        value = 0.f - read_f32(damage, 0x4C);
        std::memcpy(damage.data() + 0x4C, &value, 4);
        value = 0.f - read_f32(damage, 0x50);
        std::memcpy(damage.data() + 0x50, &value, 4);
        value = 0.f - read_f32(damage, 0x54);
        std::memcpy(damage.data() + 0x54, &value, 4);
        damage[0] = 2;
        damage[1] = 0x34;
        damage[2] = 0x65;
        const u32 one = 1;
        std::memcpy(damage.data() + 4, &one, 4);
        damage[3] = 0;
        std::memcpy(damage.data() + 0x34, &actor_address, 4);
        std::memcpy(damage.data() + 0x38, &actor_address, 4);
        std::memcpy(damage.data() + 0x3C, &actor_address, 4);
        std::memcpy(damage.data() + 0x58, &one, 4);
        const u16 actor_class = read_u16(actor.bytes, 0xCF4);
        std::memcpy(damage.data() + 0x64, &actor_class, 2);
        source_context.contacts = reply_context.contacts;
        source_context.processor.first_source = &actor;
        source_context.processor.second_source = &actor;
        source_context.processor.second_source_live = read_u32(actor.bytes, 0xCE0);
        std::array<u32, 4> result;
        receive_runtime_body_damage(source_body, damage, result, source_context);
        // Retail passes the original body to the reply, after applying damage to the source body.
        reply_context.processor.first_source = &actor;
        reply_context.processor.second_source = &actor;
        dispatch_runtime_body_damage_reply(damage, result, reply_context);
    }
}

// Xbox 0x000229F0
void request_runtime_body_timer_contact(std::span<u8> body, std::span<const u8> message,
    RuntimeBodyAttachedActor& source, std::span<const u8> source_body,
    std::span<const u8> bound, RuntimeBodyDamageEntry& context)
{
    if (read_u32(source_body, 0x760) != 0 && read_u32(source_body, 0x754) != 0 &&
        read_u32(body, 0x760) == 0 && !(read_f32(body, 0x75C) > 0.f))
    {
        receive_runtime_body_timer_contact(body, message, source, bound, context);
    }
}

// Xbox 0x00022A40
void receive_runtime_body_timer_contact(std::span<u8> body, std::span<const u8> message,
    RuntimeBodyAttachedActor& source, std::span<const u8> bound, RuntimeBodyDamageEntry& context)
{
    std::array<u8, 0x68> damage{};
    damage[0] = 2;
    damage[1] = 0x44;
    damage[2] = 0x65;
    damage[3] = 1;
    const u32 one = 1;
    std::memcpy(damage.data() + 4, &one, 4);
    std::memcpy(damage.data() + 0x18, message.data() + 0x14, 12);
    const Vec3 source_position = actor_world_position(*source.actor);
    const Vec3 target_position = actor_world_position(context.processor.actor);
    Vec3 normal{source_position.x - target_position.x, 0.f, source_position.z - target_position.z};
    const float squared = normal.z * normal.z + normal.x * normal.x;
    if (squared != 0.f)
    {
        const long double reciprocal = retail_reciprocal_sqrt(squared);
        normal.x = float(static_cast<long double>(normal.x) * reciprocal);
        normal.y = float(0.0L * reciprocal);
        normal.z = float(static_cast<long double>(normal.z) * reciprocal);
    }
    std::memcpy(damage.data() + 0x24, &normal, 12);
    std::memcpy(damage.data() + 0x40, message.data() + 8, 12);
    std::memcpy(damage.data() + 0x34, &source.address, 4);
    std::memcpy(damage.data() + 0x38, &source.address, 4);
    std::memcpy(damage.data() + 0x3C, &source.address, 4);
    Vec3 direction;
    if (context.processor.attributes.data() != nullptr && context.processor.attributes[0x1A0] != 0 && bound.data() != nullptr)
    {
        const Vec3 position = actor_world_position(*source.actor);
        direction = {read_f32(bound, 0x20) - position.x, read_f32(bound, 0x24) - position.y,
            read_f32(bound, 0x28) - position.z};
    }
    else
    {
        const Vec3 target = actor_world_position(context.processor.actor);
        const Vec3 origin = actor_world_position(*source.actor);
        direction = {target.x - origin.x, target.y - origin.y, target.z - origin.z};
    }
    float angle;
    runtime_direction_angle(direction, angle);
    direction = {float(retail_sin(angle)), 0.f, float(retail_cos(angle))};
    std::memcpy(damage.data() + 0x4C, &direction, 12);
    const u16 actor_class = read_u16(source.bytes, 0xCF4);
    std::memcpy(damage.data() + 0x64, &actor_class, 2);
    std::memcpy(damage.data() + 0x58, &one, 4);
    std::memcpy(damage.data() + 0x60, &one, 4);
    context.contacts = nullptr;
    context.processor.first_source = &source;
    context.processor.second_source = &source;
    context.processor.second_source_live = read_u32(source.bytes, 0xCE0);
    std::array<u32, 4> result;
    receive_runtime_body_damage(body, damage, result, context);
    dispatch_runtime_body_damage_reply(damage, result, context);
    if (result[1] != 0)
    {
        const float duration = 2.f; // DAT_0032B110, read from the retail image.
        std::memcpy(body.data() + 0x75C, &duration, 4);
    }
}

// Xbox 0x0001F150
void notify_runtime_body_impact_position(const Vec3& position, u32 blocked, RuntimeBodyActionMessages& context)
{
    g_actor_creation_options.flags = 0;
    g_actor_creation_options.position = position;
    g_actor_creation_options.flags = 1;
    if (blocked == 0)
    {
        if (context.events.actor_address != 0xFFFFFF74u)
        {
            dispatch_actor_attachment_records(context.event_record, 2, nullptr,
                &g_actor_creation_options, context.events, 0);
        }
    }
    else if (context.events.actor_address != 0xFFFFFF74u)
    {
        dispatch_actor_attachment_records(context.event_record, 4, nullptr,
            &g_actor_creation_options, context.events, 0);
        return;
    }
}

// Xbox 0x000231C0
void receive_runtime_body_impact(std::span<u8> body, std::span<u8> message,
    const RuntimeBodyContactHitViews& views, const u8* tag_rows, const char* const* tag_names,
    RuntimeBodyDamageEntry& context)
{
    auto& flags = context.processor.flags;
    if ((((flags.bytes[2] & 2) == 0 && (flags.bytes[4] & 0x10) == 0) ||
            read_u32(body, 0x26C) != read_u32(message, 8)) && (flags.bytes[3] & 0x10) == 0)
    {
        const u16 tag = find_runtime_player_tag_index(0x64, tag_rows, tag_names);
        std::array<u8, 0x68> damage{};
        if ((flags.bytes[4] & 0x10) == 0 && get(body, body_field::energy) != 0)
        {
            damage[0] = 1;
            damage[2] = 0x65;
        }
        else
        {
            std::memcpy(damage.data() + 0x3C, body.data() + 0x274, 4);
            damage[0] = 3;
            damage[2] = 0x68;
            const u32 one = 1;
            std::memcpy(damage.data() + 8, &one, 4);
        }
        damage[1] = 0x46;
        damage[3] = views.contacts.begin[0x24];
        const u32 mask = 0x20;
        std::memcpy(damage.data() + 4, &mask, 4);
        std::memcpy(damage.data() + 0x18, message.data() + 0x14, 12);
        std::memcpy(damage.data() + 0x30, &views.contacts_address, 4);
        Vec3 normal;
        normalized_segment_contact_direction(views.contacts, normal);
        std::memcpy(damage.data() + 0x24, &normal, 12);
        std::memcpy(damage.data() + 0x34, message.data() + 8, 4);
        const Vec3 position = actor_world_position(context.processor.actor);
        const Vec3 direction{0.f - (read_f32(message, 0x14) - position.x),
            0.f - (read_f32(message, 0x18) - position.y), 0.f - (read_f32(message, 0x1C) - position.z)};
        std::memcpy(damage.data() + 0x4C, &direction, 12);
        std::memcpy(damage.data() + 0x58, message.data() + 0xC, 4);
        const u32 one = 1;
        std::memcpy(damage.data() + 0x60, &one, 4);
        std::memcpy(damage.data() + 0x64, &tag, 2);
        context.contacts = &views.contacts;
        context.processor.first_source = &views.source;
        context.processor.second_source = nullptr;
        context.processor.second_source_live = 0;
        std::array<u32, 4> result;
        receive_runtime_body_damage(body, damage, result, context);
        Vec3 contact;
        runtime_body_damage_contact_position(damage, &views.contacts, contact);
        notify_runtime_body_impact_position(contact, result[0] == 0, context.processor.actions);
        std::memcpy(message.data() + 4, &result[1], 4);
    }
}

// Xbox 0x00022CF0
void receive_runtime_body_projectile_contact(std::span<u8> body, std::span<u8> message,
    const RuntimeBodyContactHitViews& views, RuntimeBodyDamageEntry& context)
{
    if ((context.processor.flags.bytes[7] & 0x40) == 0)
    {
        auto* host = views.source.host;
        std::array<u8, 0x68> damage{};
        damage[0] = message[4];
        damage[1] = message[8];
        damage[2] = message[0xC];
        damage[3] = views.contacts.begin[0x24];
        const u32 mask = 0x10;
        std::memcpy(damage.data() + 4, &mask, 4);
        std::memcpy(damage.data() + 0x18, message.data() + 0x28, 12);
        std::memcpy(damage.data() + 0x30, &views.contacts_address, 4);
        Vec3 normal;
        normalized_segment_contact_direction(views.contacts, normal);
        std::memcpy(damage.data() + 0x24, &normal, 12);
        std::memcpy(damage.data() + 0x34, message.data() + 0x14, 4);
        std::memcpy(damage.data() + 0x38, views.source.bytes.data() + 0xD8C, 4);
        std::memcpy(damage.data() + 0x3C, views.source.bytes.data() + 0xD8C, 4);
        const float angle = runtime_actor_angle_toward(context.processor.actor, *views.source.actor);
        const Vec3 direction{float(retail_sin(angle)), 0.f, float(retail_cos(angle))};
        std::memcpy(damage.data() + 0x4C, &direction, 12);
        u16 actor_class;
        if (host == nullptr)
        {
            actor_class = 0;
        }
        else
        {
            actor_class = read_u16(host->bytes, 0xCF4);
        }
        std::memcpy(damage.data() + 0x64, &actor_class, 2);
        const u32 one = 1;
        std::memcpy(damage.data() + 0x5C, &one, 4);
        context.contacts = &views.contacts;
        context.processor.first_source = &views.source;
        context.processor.second_source = host;
        context.processor.second_source_live = host == nullptr ? 0 : read_u32(host->bytes, 0xCE0);
        std::array<u32, 4> result;
        receive_runtime_body_damage(body, damage, result, context);
        std::memcpy(message.data() + 0x10, &result[1], 4);
    }
}

// Xbox 0x00022E40
void receive_runtime_body_moving_contact(std::span<u8> body, std::span<u8> message,
    const RuntimeBodyContactHitViews& views, RuntimeBodyDamageEntry& context)
{
    if ((context.processor.flags.bytes[7] & 0x40) == 0)
    {
        auto* host = views.source.host;
        std::array<u8, 0x68> damage{};
        damage[0] = message[4];
        damage[1] = message[8];
        damage[2] = message[0xC];
        damage[3] = views.contacts.begin[0x24];
        const u32 mask = 8;
        std::memcpy(damage.data() + 4, &mask, 4);
        std::memcpy(damage.data() + 0x18, message.data() + 0x30, 12);
        std::memcpy(damage.data() + 0x30, &views.contacts_address, 4);
        Vec3 normal;
        normalized_segment_contact_direction(views.contacts, normal);
        std::memcpy(damage.data() + 0x24, &normal, 12);
        std::memcpy(damage.data() + 0x40, message.data() + 0x20, 12);
        std::memcpy(damage.data() + 0x34, message.data() + 0x1C, 4);
        std::memcpy(damage.data() + 0x38, views.source.bytes.data() + 0xD8C, 4);
        std::memcpy(damage.data() + 0x3C, views.source.bytes.data() + 0xD8C, 4);
        std::memcpy(damage.data() + 0x4C, views.source.bytes.data() + 0x3C, 12);
        u16 actor_class;
        if (host == nullptr)
        {
            actor_class = 0;
        }
        else
        {
            actor_class = read_u16(host->bytes, 0xCF4);
        }
        std::memcpy(damage.data() + 0x64, &actor_class, 2);
        const u32 one = 1;
        std::memcpy(damage.data() + 0x5C, &one, 4);
        context.contacts = &views.contacts;
        context.processor.first_source = &views.source;
        context.processor.second_source = host;
        context.processor.second_source_live = host == nullptr ? 0 : read_u32(host->bytes, 0xCE0);
        std::array<u32, 4> result;
        receive_runtime_body_damage(body, damage, result, context);
        if (read_u32(message, 0x3C) == 0)
        {
            dispatch_runtime_body_damage_reply(damage, result, context);
        }
        std::memcpy(message.data() + 0x10, &one, 4);
        std::memcpy(message.data() + 0x14, &result[2], 4);
        const u32 standing = result[2] == 0;
        std::memcpy(message.data() + 0x18, &standing, 4);
    }
}

// Xbox 0x00022FB0
void receive_runtime_body_front_hit(std::span<u8> body, std::span<u8> message,
    const RuntimeBodyContactHitViews& views, RuntimeBodyDamageEntry& context)
{
    auto& processor = context.processor;
    if ((processor.flags.bytes[7] & 0x40) != 0)
    {
        return;
    }
    auto* host = views.source.host; // Message +0x1C's +0xD8C.
    if (host == nullptr)
    {
        throw Error("body front hit 0x00022FB0: the sender has no +0xD8C holder (retail reads its +0xCF4)");
    }
    std::array<u8, 0x68> damage{};
    const u32 health = get(body, body_field::energy);
    const u16 actor_class = read_u16(host->bytes, 0xCF4);
    // Retail leaves this slot (the 0x1A message's +8) unwritten on the class path.
    u32 actor_amount = 0;
    i32 amount;
    if ((processor.actor.mask_mode == 2 || processor.actor.mask_flag != 0) && actor_class != 0)
    {
        amount = i32(runtime_class_damage_value(0x41, actor_class, processor.tables)); // 0x00133BD0; CVTTSS2SI
    }
    else
    {
        actor_amount = u32(runtime_actor_damage_value(0x22, processor.actor.damage_class, processor.tables)); // 0x00028DA0
        const bool live = runtime_actor_is_live_player(*host->actor, read_u32(host->bytes, 0xCE0)) // 0x00028D20
            && processor.player_damage != 0;
        amount = i32(actor_amount);
        if (actor_amount != 0 && live)
        {
            amount = i32(health);
        }
    }
    if ((processor.flags.bytes[8] & 1) == 0 && i32(health) > amount)
    {
        damage[0] = 0xD;
        damage[1] = 0x42;
        damage[2] = 0x67;
        // The stack words from +0x10: 0x1A, the health, the 0x00028DA0 amount, then this damage packet.
        std::array<u32, 3 + 0x1A> words{};
        words[0] = 0x1A;
        words[1] = health;
        words[2] = actor_amount;
        std::memcpy(words.data() + 3, damage.data(), damage.size());
        processor.actions.actor_message(words); // Actor vtable +0xC
    }
    else
    {
        damage[0] = 0xC;
        damage[1] = 0x42;
        damage[2] = 0x68;
    }
    damage[3] = views.contacts.begin[0x24];
    const u32 mask = 8;
    std::memcpy(damage.data() + 4, &mask, 4);
    std::memcpy(damage.data() + 0x18, message.data() + 0x30, 12);
    std::memcpy(damage.data() + 0x30, &views.contacts_address, 4);
    Vec3 normal;
    normalized_segment_contact_direction(views.contacts, normal); // 0x000FCA60
    std::memcpy(damage.data() + 0x24, &normal, 12);
    std::memcpy(damage.data() + 0x40, message.data() + 0x20, 12);
    std::memcpy(damage.data() + 0x34, message.data() + 0x1C, 4);
    std::memcpy(damage.data() + 0x38, views.source.bytes.data() + 0xD8C, 4);
    std::memcpy(damage.data() + 0x3C, views.source.bytes.data() + 0xD8C, 4);
    std::memcpy(damage.data() + 0x4C, views.source.bytes.data() + 0x3C, 12);
    std::memcpy(damage.data() + 0x64, &actor_class, 2);
    const u32 one = 1;
    std::memcpy(damage.data() + 0x5C, &one, 4);
    context.contacts = &views.contacts;
    processor.first_source = &views.source;
    processor.second_source = host;
    processor.second_source_live = read_u32(host->bytes, 0xCE0);
    std::array<u32, 4> result;
    receive_runtime_body_damage(body, damage, result, context); // 0x00020DC0
    dispatch_runtime_body_damage_reply(damage, result, context); // 0x00020F80
    std::memcpy(message.data() + 0x10, &one, 4);
    std::memcpy(message.data() + 0x14, &result[2], 4);
    const u32 standing = result[2] == 0;
    std::memcpy(message.data() + 0x18, &standing, 4);
}

// Xbox 0x000233A0
void receive_runtime_body_contact_health_hit(std::span<u8> body, std::span<u8> message,
    const RuntimeBodyContactHitViews& views, RuntimeBodyDamageEntry& context)
{
    if ((context.processor.flags.bytes[7] & 0x40) == 0 &&
        !runtime_body_latches_block(body, context.processor.actor, 0x3F))
    {
        std::array<u8, 0x68> damage{};
        damage[0] = 0x13;
        damage[1] = 0x32;
        damage[2] = 0x64;
        damage[3] = views.contacts.begin[0x24];
        const u32 one = 1;
        std::memcpy(damage.data() + 4, &one, 4);
        std::memcpy(damage.data() + 0x18, message.data() + 0x10, 12);
        std::memcpy(damage.data() + 0x30, &views.contacts_address, 4);
        Vec3 normal;
        normalized_segment_contact_direction(views.contacts, normal);
        std::memcpy(damage.data() + 0x24, &normal, 12);
        std::memcpy(damage.data() + 0x34, message.data() + 8, 4);
        if (read_u32(body, 0x278) == 0)
        {
            std::memcpy(damage.data() + 0x38, body.data() + 0x270, 4);
            std::memcpy(damage.data() + 0x3C, body.data() + 0x274, 4);
        }
        const Vec3 position = actor_world_position(context.processor.actor);
        const Vec3 direction{0.f - (read_f32(message, 0x10) - position.x),
            0.f - (read_f32(message, 0x14) - position.y), 0.f - (read_f32(message, 0x18) - position.z)};
        std::memcpy(damage.data() + 0x4C, &direction, 12);
        std::memcpy(damage.data() + 0x58, &one, 4);
        std::memcpy(damage.data() + 0x60, &one, 4);
        context.contacts = &views.contacts;
        context.processor.first_source = &views.source;
        context.processor.second_source = read_u32(damage, 0x38) == 0 ? nullptr : views.retained_source;
        context.processor.second_source_live = read_u32(damage, 0x38) == 0 ? 0 : read_u32(views.retained_source->bytes, 0xCE0);
        std::array<u32, 4> result;
        receive_runtime_body_damage(body, damage, result, context);
        if (result[1] != 0)
        {
            Vec3 contact;
            runtime_body_damage_contact_position(damage, &views.contacts, contact);
            g_actor_creation_options.position = contact;
            g_actor_creation_options.flags = 1;
            auto& actions = context.processor.actions;
            if (read_u32(body, 8) != 0xFFFFFF74u)
            {
                dispatch_actor_attachment_records(actions.event_record, 2, nullptr,
                    &g_actor_creation_options, actions.events, 0);
            }
        }
        std::memcpy(message.data() + 4, &result[1], 4);
    }
}

// Xbox 0x00023580
void receive_runtime_body_contact_knockdown(std::span<u8> body, std::span<u8> message,
    const RuntimeBodyContactHitViews& views, RuntimeBodyDamageEntry& context)
{
    if ((context.processor.flags.bytes[7] & 0x40) == 0)
    {
        std::array<u8, 0x68> damage{};
        damage[0] = 5;
        damage[1] = 0x32;
        damage[2] = 0x68;
        damage[3] = views.contacts.begin[0x24];
        const u32 one = 1;
        std::memcpy(damage.data() + 4, &one, 4);
        std::memcpy(damage.data() + 0xC, &one, 4);
        std::memcpy(damage.data() + 0x10, &one, 4);
        std::memcpy(damage.data() + 0x18, message.data() + 0x10, 12);
        std::memcpy(damage.data() + 0x30, &views.contacts_address, 4);
        Vec3 normal;
        normalized_segment_contact_direction(views.contacts, normal);
        std::memcpy(damage.data() + 0x24, &normal, 12);
        std::memcpy(damage.data() + 0x34, message.data() + 8, 4);
        const Vec3 position = actor_world_position(context.processor.actor);
        const Vec3 direction{0.f - (read_f32(message, 0x10) - position.x),
            0.f - (read_f32(message, 0x14) - position.y), 0.f - (read_f32(message, 0x18) - position.z)};
        std::memcpy(damage.data() + 0x4C, &direction, 12);
        std::memcpy(damage.data() + 0x60, &one, 4);
        context.contacts = &views.contacts;
        context.processor.first_source = &views.source;
        context.processor.second_source = nullptr;
        context.processor.second_source_live = 0;
        std::array<u32, 4> result;
        receive_runtime_body_damage(body, damage, result, context);
        if (result[1] != 0)
        {
            Vec3 contact;
            runtime_body_damage_contact_position(damage, &views.contacts, contact);
            g_actor_creation_options.position = contact;
            g_actor_creation_options.flags = 1;
            auto& actions = context.processor.actions;
            if (read_u32(body, 8) != 0xFFFFFF74u)
            {
                dispatch_actor_attachment_records(actions.event_record, 2, nullptr,
                    &g_actor_creation_options, actions.events, 0);
            }
        }
        std::memcpy(message.data() + 4, &result[1], 4);
    }
}

// Xbox 0x00021010
void runtime_body_damage_contact_position(std::span<const u8> damage,
    const entity_detail::SegmentPool* contacts, Vec3& output)
{
    if (contacts != nullptr)
    {
        const std::span<const u8> row{contacts->begin, 0x54};
        float y = read_f32(damage, 0x1C) - read_f32(row, 0x30);
        float z = read_f32(damage, 0x20) - read_f32(row, 0x34);
        const float radius = read_f32(row, 0x38) + 1.f;
        float x = read_f32(damage, 0x18) - read_f32(row, 0x2C);
        const float squared = (y * y + z * z) + x * x;
        if (squared != 0.f)
        {
            const long double factor = retail_reciprocal_sqrt(squared) * static_cast<long double>(radius);
            x = float(static_cast<long double>(x) * factor);
            y = float(static_cast<long double>(y) * factor);
            z = float(static_cast<long double>(z) * factor);
        }
        output.x = read_f32(row, 0x2C) + x;
        output.y = read_f32(row, 0x30) + y;
        output.z = read_f32(row, 0x34) + z;
        return;
    }
    output = {read_f32(damage, 0x18), read_f32(damage, 0x1C), read_f32(damage, 0x20)};
}

// Xbox 0x0001F1D0
void notify_runtime_body_lethal_hit_position(const Vec3& position, RuntimeBodyActionMessages& context)
{
    g_actor_creation_options.flags = 0;
    g_actor_creation_options.position = position;
    g_actor_creation_options.scale = 1.f;
    g_actor_creation_options.flags = 0x41;
    if (context.events.actor_address != 0xFFFFFF74u)
    {
        dispatch_actor_attachment_records(context.event_record, 8, nullptr,
            &g_actor_creation_options, context.events, 0);
    }
}

// Xbox 0x0001F240
void notify_runtime_body_contact_hit_position(const Actor* source, const Vec3& position,
    u32 knockdown, u32 blocked, std::span<const u8> attributes, RuntimeBodyActionMessages& context)
{
    if (blocked != 0)
    {
        dispatch_runtime_body_position_event(position, i32(knockdown != 0) + 4, context);
        return;
    }
    bool special;
    if (source == nullptr)
    {
        special = context.attachment.ownership.actor.mask_mode == 1;
    }
    else
    {
        special = true;
        if (context.attachment.ownership.actor.mask_mode != 1 || source->mask_mode == 1)
        {
            special = false;
        }
    }
    float scale = 1.f;
    if (attributes.data() != nullptr)
    {
        scale = read_f32(attributes, 0x190);
    }
    if (!special)
    {
        dispatch_runtime_body_scaled_position_event(position, i32(knockdown != 0) + 2, scale, context);
        return;
    }
    dispatch_runtime_body_scaled_position_event(position, i32(knockdown != 0) + 6, scale, context);
}

// Xbox 0x00020F80
void dispatch_runtime_body_damage_reply(std::span<const u8> damage,
    std::span<const u32, 4> result, RuntimeBodyDamageEntry& context)
{
    Vec3 position;
    runtime_body_damage_contact_position(damage, context.contacts, position);
    if (result[1] != 0 && damage[0] == 0x12)
    {
        notify_runtime_body_lethal_hit_position(position, context.processor.actions);
        return;
    }
    if (read_u32(damage, 0x38) != 0)
    {
        notify_runtime_body_contact_hit_position(context.processor.second_source->actor,
            position, result[2], result[0] == 0, context.processor.attributes, context.processor.actions);
        return;
    }
    if (read_u32(damage, 0x5C) != 0)
    {
        notify_runtime_body_contact_hit_position(context.processor.first_source != nullptr ? context.processor.first_source->actor : nullptr,
            position, result[2], result[0] == 0, context.processor.attributes, context.processor.actions);
    }
}

// Xbox 0x0013ADC0
void dispatch_runtime_body_scaled_position_event(const Vec3& position, i32 id,
    float scale, RuntimeBodyActionMessages& context)
{
    g_actor_creation_options.flags = 0;
    g_actor_creation_options.position = position;
    g_actor_creation_options.scale = scale;
    g_actor_creation_options.flags = 0x41;
    if (context.events.actor_address == 0xFFFFFF74u)
    {
        return;
    }
    dispatch_actor_attachment_records(context.event_record, id, nullptr,
        &g_actor_creation_options, context.events, 0);
}

// Xbox 0x0001F0B0
void notify_runtime_body_hit_position(const Actor* source, const Vec3& position,
    u32 blocked, std::span<const u8> attributes, RuntimeBodyActionMessages& context)
{
    if (blocked != 0)
    {
        dispatch_runtime_body_position_event(position, 4, context);
        return;
    }
    bool special;
    if (source == nullptr)
    {
        special = context.attachment.ownership.actor.mask_mode == 1;
    }
    else
    {
        special = true;
        if (context.attachment.ownership.actor.mask_mode != 1 || source->mask_mode == 1)
        {
            special = false;
        }
    }
    float scale = 1.f;
    if (attributes.data() != nullptr)
    {
        scale = read_f32(attributes, 0x190);
    }
    if (!special)
    {
        dispatch_runtime_body_scaled_position_event(position, 2, scale, context);
        return;
    }
    dispatch_runtime_body_scaled_position_event(position, 6, scale, context);
}

// Xbox 0x000205B0
void receive_runtime_body_direct_hit(std::span<u8> body, RuntimeBodyAttachedActor& source,
    const Vec3& notice_position, RuntimeBodyDamageEntry& context)
{
    std::array<u8, 0x68> damage{};
    damage[0] = 1;
    damage[1] = 0x45;
    damage[2] = 0x64;
    damage[3] = 0xE;
    const u32 one = 1;
    std::memcpy(damage.data() + 4, &one, 4);
    const Vec3 position = actor_world_position(*source.actor);
    std::memcpy(damage.data() + 0x18, &position, 12);
    const u16 actor_class = read_u16(source.bytes, 0xCF4);
    std::memcpy(damage.data() + 0x64, &actor_class, 2);
    std::memcpy(damage.data() + 0x60, &one, 4);
    const float direction_y = 1.f;
    std::memcpy(damage.data() + 0x2C, &direction_y, 4);
    std::memcpy(damage.data() + 0x34, &source.address, 4);
    std::memcpy(damage.data() + 0x38, &source.address, 4);
    std::memcpy(damage.data() + 0x3C, &source.address, 4);
    // Relocated native views of the three source words in this stack damage packet.
    context.contacts = nullptr;
    context.processor.first_source = &source;
    context.processor.second_source = &source;
    context.processor.second_source_live = read_u32(source.bytes, 0xCE0);
    std::array<u32, 4> result;
    receive_runtime_body_damage(body, damage, result, context);
    notify_runtime_body_hit_position(source.actor, notice_position, result[0] == 0,
        context.processor.attributes, context.processor.actions);
}

// The knock-back hit 0x000C1920 sends a body's own actor: a reaction hit (1, 0x32, 0x65) at its position, facing
// along its horizontal movement (actor +0x3C), pushed along the body's horizontal +0x1D8 heading, its source the first
// background (DAT_00511B20 - 0x934). Either direction falls back to straight up when it has no horizontal part.
// Xbox 0x00020690
void receive_runtime_body_knock_back(std::span<u8> body, const Actor& actor,
    std::span<const u8> actor_bytes, RuntimeBodyAttachedActor* background,
    RuntimeBodyDamageEntry& context)
{
    const Vec3 movement{read_f32(actor_bytes, 0x3C), read_f32(actor_bytes, 0x40), read_f32(actor_bytes, 0x44)};
    constexpr float kUp = 1.f; // DAT_0032B0F8.
    Vec3 facing{movement.x, 0.f, movement.z};
    if (facing.x == 0.f && facing.z == 0.f)
    {
        facing = Vec3{0.f, kUp, 0.f};
    }
    float length = facing.z * facing.z + facing.y * facing.y + facing.x * facing.x;
    if (length != 0.f)
    {
        const float inverse = engine_reciprocal_sqrt(length); // 0x001534C0
        facing.x = facing.x * inverse;
        facing.y = facing.y * inverse;
        facing.z = facing.z * inverse;
    }
    Vec3 push{read_f32(body, 0x1D8), 0.f, read_f32(body, 0x1E0)};
    if (push.x == 0.f && push.z == 0.f)
    {
        push = Vec3{0.f, kUp, 0.f};
    }
    length = push.z * push.z + push.y * push.y + push.x * push.x;
    if (length != 0.f)
    {
        const float inverse = engine_reciprocal_sqrt(length); // 0x001534C0
        push.x = push.x * inverse;
        push.y = push.y * inverse;
        push.z = push.z * inverse;
    }
    std::array<u8, 0x68> damage{};
    damage[0] = 1;
    damage[1] = 0x32;
    damage[2] = 0x65;
    damage[3] = 0;
    const u32 one = 1;
    std::memcpy(damage.data() + 4, &one, 4);
    const Vec3 position = actor_world_position(actor); // 0x000412A0
    std::memcpy(damage.data() + 0x18, &position, 12);
    const u32 source = background == nullptr ? 0 : background->address;
    std::memcpy(damage.data() + 0x24, &facing, 12);
    std::memcpy(damage.data() + 0x34, &source, 4);
    std::memcpy(damage.data() + 0x40, &movement, 12);
    std::memcpy(damage.data() + 0x4C, &push, 12);
    // Native views of the damage's +0x30 contacts and its +0x34 / +0x38 sources.
    context.contacts = nullptr;
    context.processor.first_source = background;
    context.processor.second_source = nullptr;
    context.processor.second_source_live = 0;
    std::array<u32, 4> result;
    receive_runtime_body_damage(body, damage, result, context); // 0x00020DC0
}

// Xbox 0x00023740
void receive_runtime_body_health_notice(std::span<u8> body, std::span<u8> message,
    std::span<const char* const, 4> first_names,
    std::span<const char* const, 4> second_names, RuntimeBodyDamageEntry& context)
{
    const u32 actor = read_u32(body, 8);
    const u32 amount = read_u32(message, 8);
    if (amount != 0)
    {
        const i32 remaining = std::bit_cast<i32>(u32(get(body, body_field::energy)) - amount);
        const u16 health = u16(remaining) & u16((remaining < 1) - 1);
        std::memcpy(body.data() + 0x94, &health, 2);
    }
    const u32 kind = read_u32(message, 0xC);
    if (read_u32(message, 0x10) == 0)
    {
        switch (kind)
        {
        case 0:
        case 1:
        case 2:
        case 3:
            select_runtime_body_animation(first_names[kind], context.processor.actions.actions);
            break;
        }
    }
    else
    {
        switch (kind)
        {
        case 0:
        case 1:
        case 2:
        case 3:
            select_runtime_body_animation(second_names[kind], context.processor.actions.actions);
            break;
        }
    }
    const u32 zero = 0;
    std::memcpy(body.data() + 0x240, &zero, 4);
    std::memcpy(body.data() + 0x23C, &zero, 4);
    const u32 notice[]{0x61, 0x7E};
    context.processor.actions.actor_message(notice);
    auto& actions = context.processor.actions.actions;
    if (actions.table != nullptr)
    {
        u32 output;
        const u8* row = find_runtime_script_table_row(actions.table, 0x7E, output);
        if (row != nullptr)
        {
            select_runtime_body_action_row(body, row, nullptr, actions);
        }
    }
    const u32 updated_amount = read_u32(message, 8);
    notify_runtime_body_damage(context.game, actor, 0, updated_amount, 0, 0, 0, 0, updated_amount == 0);
    const u32 handled = 1;
    std::memcpy(message.data() + 4, &handled, 4);
}

// Xbox 0x00134E50
void request_runtime_body_lethal_action(std::span<u8> body, RuntimeBodyActionSelection& actions)
{
    if (get(body, body_field::energy) != 0)
    {
        u32 output;
        const u16 zero = 0;
        if (actions.table == nullptr || find_runtime_script_table_row(actions.table, 0x6B, output) == nullptr ||
            get(body, body_field::move_holds) != 0 || get(body, body_field::move_hold_time) != 0.f)
        {
            std::memcpy(body.data() + 0x94, &zero, 2);
        }
        else
        {
            std::memcpy(body.data() + 0x94, &zero, 2);
            if (actions.table != nullptr)
            {
                const u8* row = find_runtime_script_table_row(actions.table, 0x6B, output);
                if (row != nullptr)
                {
                    select_runtime_body_action_row(body, row, nullptr, actions);
                    return;
                }
            }
        }
    }
}
}
