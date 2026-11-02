#include "body_source_contacts.h"
#include "segment_records.h"
#include "body_action_ids.h"
#include "body_fields.h"
#include <cstring>

namespace gbtg
{
// Xbox 0x00020020
void receive_runtime_body_source_contact(std::span<u8> body, std::span<u8> message,
    RuntimeBodySourceContact& context)
{
    auto& processor = context.target.processor;
    if ((processor.flags.bytes[7] & 0x40) != 0)
    {
        return;
    }
    auto& source = context.contacts.source;
    std::array<u8, 0x68> damage{};
    if (context.hit.data() == nullptr)
    {
        damage[0] = 0;
        damage[1] = 0x33;
        damage[2] = 0x65;
    }
    else
    {
        damage[0] = context.hit[3];
        damage[1] = context.hit[4];
        damage[2] = context.hit[5];
    }
    damage[3] = context.contacts.contacts.begin[0x24];
    const u32 mask = u32(damage[0] == 3) + 1;
    std::memcpy(damage.data() + 4, &mask, 4);
    std::memcpy(damage.data() + 0x18, message.data() + 0x1C, 12);
    std::memcpy(damage.data() + 0x30, &context.contacts.contacts_address, 4);
    Vec3 normal;
    normalized_segment_contact_direction(context.contacts.contacts, normal);
    std::memcpy(damage.data() + 0x24, &normal, 12);
    std::memcpy(damage.data() + 0x40, message.data() + 0xC, 12);
    u32 flags;
    if (context.hit.data() == nullptr)
    {
        flags = 0;
    }
    else
    {
        flags = read_u32(context.hit, 0x20);
    }
    if ((flags & 8) != 0)
    {
        bool can_override = true;
        if (context.goals != nullptr)
        {
            const u8* row;
            if (context.goals->tail == context.goals->capacity)
            {
                row = context.address_zero.data();
            }
            else
            {
                row = context.goals->rows.data() + size_t(context.goals->head) * context.goals->stride;
            }
            u16 kind;
            std::memcpy(&kind, row, 2);
            if (kind != 0)
            {
                can_override = false;
            }
        }
        if (can_override && get(body, body_field::holds) == 0 && (processor.flags.bytes[7] & 2) == 0)
        {
            const u32 one = 1;
            std::memcpy(damage.data() + 0x10, &one, 4);
        }
    }
    if (context.hit.data() == nullptr)
    {
        flags = 0;
    }
    else
    {
        flags = read_u32(context.hit, 0x20);
    }
    std::memcpy(damage.data() + 0x34, &source.address, 4);
    std::memcpy(damage.data() + 0x38, &source.address, 4);
    std::memcpy(damage.data() + 0x3C, &source.address, 4);
    Vec3 direction;
    if ((flags & 1) == 0)
    {
        if (processor.attributes.data() != nullptr && processor.attributes[0x1A0] != 0 && context.bound.data() != nullptr)
        {
            const float angle = runtime_actor_bound_angle(context.bound, *source.actor);
            runtime_horizontal_direction(direction, angle, 1.f);
        }
        else
        {
            const float angle = runtime_actor_angle_toward(processor.actor, *source.actor);
            direction = {float(retail_sin(angle)), 0.f, float(retail_cos(angle))};
        }
    }
    else
    {
        const float angle = runtime_body_hit_angle(context.hit) + read_f32(source.bytes, 0x18);
        direction = {float(retail_sin(angle)), 0.f, float(retail_cos(angle))};
    }
    std::memcpy(damage.data() + 0x4C, &direction, 12);
    u32 timer;
    if (read_u32(context.source_body, 0x760) == 0)
    {
        timer = 0;
    }
    else
    {
        timer = read_u32(context.source_body, 0x754);
    }
    std::memcpy(damage.data() + 0x58, &timer, 4);
    const u16 actor_class = read_u16(source.bytes, 0xCF4);
    std::memcpy(damage.data() + 0x64, &actor_class, 2);
    const u32 one = 1;
    std::memcpy(damage.data() + 0x60, &one, 4);
    context.target.contacts = &context.contacts.contacts;
    processor.first_source = &source;
    processor.second_source = &source;
    processor.second_source_live = read_u32(source.bytes, 0xCE0);
    std::array<u32, 4> result;
    receive_runtime_body_damage(body, damage, result, context.target);
    dispatch_runtime_body_damage_reply(damage, result, context.target);
    std::memcpy(message.data() + 4, &one, 4);
    reflect_runtime_body_timer_hit(body, context.actor, context.source_body, damage, context.source, context.target);
}

// Xbox 0x000240C0
void receive_runtime_body_owned_contact(std::span<u8> body, std::span<u8> message,
    RuntimeBodySourceContact& context)
{
    auto& processor = context.target.processor;
    if ((processor.flags.bytes[7] & 0x40) != 0)
    {
        return;
    }
    auto& source = context.contacts.source;
    auto* host = source.host;
    std::array<u8, 0x68> damage{};
    damage[0] = message[4];
    damage[1] = message[8];
    damage[2] = message[0xC];
    damage[3] = context.contacts.contacts.begin[0x24];
    const u32 mask = 4;
    std::memcpy(damage.data() + 4, &mask, 4);
    if (host != nullptr)
    {
        const u32 value = classify_runtime_attached_owner(source.bytes, host->actor) == 4;
        std::memcpy(damage.data() + 0x10, &value, 4);
    }
    std::memcpy(damage.data() + 0x18, message.data() + 0x28, 12);
    std::memcpy(damage.data() + 0x30, &context.contacts.contacts_address, 4);
    Vec3 normal;
    normalized_segment_contact_direction(context.contacts.contacts, normal);
    std::memcpy(damage.data() + 0x24, &normal, 12);
    std::memcpy(damage.data() + 0x40, message.data() + 0x18, 12);
    std::memcpy(damage.data() + 0x34, message.data() + 0x14, 4);
    std::memcpy(damage.data() + 0x38, source.bytes.data() + 0xD8C, 4);
    std::memcpy(damage.data() + 0x3C, source.bytes.data() + 0xD8C, 4);
    // This packet's source hit view belongs to the host actor's body +0x84.
    u32 flags;
    if (context.hit.data() == nullptr)
    {
        flags = 0;
    }
    else
    {
        flags = read_u32(context.hit, 0x20);
    }
    Vec3 direction;
    if ((flags & 1) == 0)
    {
        if (processor.attributes.data() != nullptr && processor.attributes[0x1A0] != 0 && context.bound.data() != nullptr)
        {
            const float angle = runtime_actor_bound_angle(context.bound, *host->actor);
            runtime_horizontal_direction(direction, angle, 1.f);
        }
        else
        {
            const float angle = runtime_actor_angle_toward(processor.actor, *host->actor);
            direction = {float(retail_sin(angle)), 0.f, float(retail_cos(angle))};
        }
    }
    else
    {
        const float angle = runtime_body_hit_angle(context.hit) + read_f32(host->bytes, 0x18);
        direction = {float(retail_sin(angle)), 0.f, float(retail_cos(angle))};
    }
    std::memcpy(damage.data() + 0x4C, &direction, 12);
    const u16 actor_class = read_u16(host->bytes, 0xCF4);
    std::memcpy(damage.data() + 0x64, &actor_class, 2);
    const u32 one = 1;
    std::memcpy(damage.data() + 0x5C, &one, 4);
    std::memcpy(damage.data() + 0x60, &one, 4);
    context.target.contacts = &context.contacts.contacts;
    processor.first_source = &source;
    processor.second_source = host;
    processor.second_source_live = read_u32(host->bytes, 0xCE0);
    std::array<u32, 4> result;
    receive_runtime_body_damage(body, damage, result, context.target);
    dispatch_runtime_body_damage_reply(damage, result, context.target);
    std::memcpy(message.data() + 0x10, &one, 4);
}
}
