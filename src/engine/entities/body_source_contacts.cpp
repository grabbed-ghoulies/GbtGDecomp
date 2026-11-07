#include "body_source_contacts.h"
#include "body_damage_fields.h"
#include "segment_records.h"
#include "body_action_ids.h"
#include "body_fields.h"
#include "player_fields.h"
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
    BodyDamage damage{};
    if (context.hit.data() == nullptr)
    {
        damage.kind = 0;
        damage.key = 0x33;
        damage.push_key = 0x65;
    }
    else
    {
        damage.kind = context.hit[3];
        damage.key = context.hit[4];
        damage.push_key = context.hit[5];
    }
    damage.joint = context.contacts.contacts.begin[0x24];
    const u32 mask = u32(damage.kind == 3) + 1;
    damage.latch_mask = mask;
    std::memcpy(&damage.position, message.data() + 0x1C, 12);
    damage.contacts = context.contacts.contacts_address;
    Vec3 normal;
    normalized_segment_contact_direction(context.contacts.contacts, normal);
    damage.normal = normal;
    std::memcpy(&damage.push, message.data() + 0xC, 12);
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
        if (can_override && body_record(body).holds == 0 && (processor.flags.bytes[7] & 2) == 0)
        {
            const u32 one = 1;
            damage.unblockable = one;
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
    damage.source = source.address;
    damage.attacker = source.address;
    damage.holder = source.address;
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
    damage.direction = direction;
    u32 timer;
    if (body_record(context.source_body).hit_timer == 0)
    {
        timer = 0;
    }
    else
    {
        timer = body_record(context.source_body).hit_timer_running;
    }
    damage.starts_timer = timer;
    const u16 actor_class = player_record(source.bytes).damage_class;
    damage.source_class = actor_class;
    const u32 one = 1;
    damage.unk_60 = one;
    context.target.contacts = &context.contacts.contacts;
    processor.first_source = &source;
    processor.second_source = &source;
    processor.second_source_live = player_record(source.bytes).live;
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
    BodyDamage damage{};
    damage.kind = message[4];
    damage.key = message[8];
    damage.push_key = message[0xC];
    damage.joint = context.contacts.contacts.begin[0x24];
    const u32 mask = 4;
    damage.latch_mask = mask;
    if (host != nullptr)
    {
        const u32 value = classify_runtime_attached_owner(source.bytes, host->actor) == 4;
        damage.unblockable = value;
    }
    std::memcpy(&damage.position, message.data() + 0x28, 12);
    damage.contacts = context.contacts.contacts_address;
    Vec3 normal;
    normalized_segment_contact_direction(context.contacts.contacts, normal);
    damage.normal = normal;
    std::memcpy(&damage.push, message.data() + 0x18, 12);
    std::memcpy(&damage.source, message.data() + 0x14, 4);
    std::memcpy(&damage.attacker, source.bytes.data() + 0xD8C, 4);
    std::memcpy(&damage.holder, source.bytes.data() + 0xD8C, 4);
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
    damage.direction = direction;
    const u16 actor_class = player_record(host->bytes).damage_class;
    damage.source_class = actor_class;
    const u32 one = 1;
    damage.unk_5C = one;
    damage.unk_60 = one;
    context.target.contacts = &context.contacts.contacts;
    processor.first_source = &source;
    processor.second_source = host;
    processor.second_source_live = player_record(host->bytes).live;
    std::array<u32, 4> result;
    receive_runtime_body_damage(body, damage, result, context.target);
    dispatch_runtime_body_damage_reply(damage, result, context.target);
    std::memcpy(message.data() + 0x10, &one, 4);
}
}
