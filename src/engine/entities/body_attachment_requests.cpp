#include "body_attachment_requests.h"
#include "weapon_fields.h"
#include <cstddef>
#include "player_mind_targets.h"
#include "body_message_helpers.h"
#include "shape_copy.h"
#include "body_fields.h"
#include <bit>
#include <cstring>

namespace gbtg
{
// Xbox 0x0001CD00
u32 runtime_body_target_height(std::span<const u8> body, const Actor& actor,
    std::span<const u8> target, u8* shape)
{
    if (shape != nullptr)
    {
        copy_actor_shape(shape, target.data() + 0x1C);
    }
    const float height = actor.radius_y * body_record(body).height_scale;
    const float radius = get(target, mind_target_field::shape_height);
    const Vec3 position = actor_world_position(actor);
    const float difference = (get(target, mind_target_field::shape_position).y + radius) - (position.y + height);
    if (difference > height * .25f)
    {
        return 2;
    }
    if (height * -.25f > difference)
    {
        return 0;
    }
    return 1;
}

// Xbox 0x0001D180
bool runtime_body_accepts_target_request(std::span<const u8> body,
    const EntityBitBlock& flags, u16 owner_class)
{
    if (body_record(body).unk_6B == 0 && (body_record(body).unk_6A == 0 || body_record(body).unk_6C != 0)
        && !(body_record(body).unk_70 > 0.f) && body_record(body).move_holds == 0
        && body_record(body).move_hold_time == 0.f && (flags.bytes[3] & 8) == 0)
    {
        if (!runtime_body_bit_set(flags, 0x2C))
        {
            if (!runtime_body_bit_set(flags, 0x2A)
                && (owner_class != 0 || (flags.bytes[5] & 2) != 0))
            {
                return true;
            }
        }
    }
    return false;
}

namespace
{
const u8* search(const RuntimeAttachmentRequestRows& rows, u16 key, size_t stride)
{
    i32 high = i32(rows.count) - 1;
    i32 low = 0;
    if (high >= 0)
    {
        do
        {
            const i32 middle = (low + high) >> 1;
            const u8* row = rows.rows + size_t(middle) * stride;
            const u16 candidate = read_u16({row, stride}, 0);
            if (candidate < key)
            {
                low = middle + 1;
            }
            else
            {
                if (candidate <= key)
                {
                    return row;
                }
                high = middle - 1;
            }
        } while (low <= high);
    }
    return nullptr;
}
}

// Xbox 0x0001C560
const u8* find_runtime_attachment_first_request(const RuntimeAttachmentRequestTables& tables, u16 key)
{
    return search(tables.first, key, 0x24);
}

// Xbox 0x0001C5A0
const u8* find_runtime_attachment_second_request(const RuntimeAttachmentRequestTables& tables, u16 key)
{
    return search(tables.second, key, 0x24);
}

// Xbox 0x0001CB20
const u8* find_runtime_attachment_pair_request(const RuntimeAttachmentRequestTables& tables, u16 key)
{
    return search(tables.pair, key, 0x28);
}

// Xbox 0x0001C7C0
const u8* find_runtime_attachment_category_request(const RuntimeAttachmentRequestTables& tables, u16 key)
{
    return search(tables.category, key, 0x2C);
}

// Xbox 0x0001B170
u16 runtime_body_attached_class(const RuntimeBodyAttachedActor* attached, u16 owner_class)
{
    if (attached != nullptr)
    {
        return classify_runtime_attached_actor(attached->bytes, owner_class);
    }
    return 0;
}

// Xbox 0x0001B150
u8 runtime_body_attached_category(const RuntimeBodyAttachedActor* attached)
{
    if (attached != nullptr)
    {
        return weapon_base(attached->bytes).unk_93D;
    }
    return 0;
}

// Xbox 0x0001C800
bool runtime_attachment_request_matches(std::span<const u8> row,
    const RuntimeBodyAttachedActor* primary, const RuntimeBodyAttachedActor* secondary, u16 owner_class)
{
    u32 category;
    u32 classification;
    if (secondary != nullptr)
    {
        classification = classify_runtime_attached_actor(secondary->bytes, owner_class);
        category = weapon_base(secondary->bytes).unk_93D;
    }
    else
    {
        if (primary != nullptr)
        {
            classification = classify_runtime_attached_actor(primary->bytes, owner_class);
        }
        else
        {
            classification = 0;
        }
        category = primary == nullptr ? 0 : primary->bytes[0x93D];
    }
    if (read_u32(row, 0x24) != 0 && read_u32(row, 0x24) != classification)
    {
        return false;
    }
    return read_u32(row, 0x28) == category;
}

// Xbox 0x0001C930
const u8* find_runtime_attachment_special_request(const RuntimeAttachmentRequestTables* tables,
    const RuntimeBodyAttachedActor* primary, u16 key)
{
    if (primary != nullptr && primary->bytes[0x93D] == 4 && tables != nullptr &&
        tables->special.rows != nullptr)
    {
        return search(tables->special, key, 0x24);
    }
    return nullptr;
}

// Xbox 0x0001D2E0
bool accept_runtime_attachment_request(RuntimeBodyAttachmentRequests& context, const u8* row, u32 timer)
{
    const u32 address = context.address_of(row);
    context.state = row;
   body_record(context.body).hit_record = address;
   body_record(context.body).unk_88 = timer;
    const u16 key = read_u16({row, 2}, 0);
    if (key != 0 && context.actions.table != nullptr)
    {
        u32 optional;
        const u8* action = find_runtime_script_table_row(context.actions.table, key, optional);
        if (action != nullptr)
        {
            select_runtime_body_action_row(context.body, action, nullptr, context.actions);
           body_record(context.body).unk_88 = timer;
            context.state = row;
           body_record(context.body).hit_record = address;
            return true;
        }
    }
   body_record(context.body).unk_88 = timer;
    context.state = row;
   body_record(context.body).hit_record = address;
    return false;
}

// Xbox 0x0001D210
bool select_runtime_attachment_request(RuntimeBodyAttachmentRequests& context, u16 key)
{
    const u8* row;
    const auto* tables = context.tables;
    if (tables != nullptr)
    {
        if (tables->first.rows != nullptr)
        {
            row = find_runtime_attachment_first_request(*tables, key);
            if (row != nullptr)
            {
                accept_runtime_attachment_request(context, row, 0);
                return true;
            }
        }
        if (tables->second.rows != nullptr)
        {
            row = find_runtime_attachment_second_request(*tables, key);
            if (row != nullptr)
            {
                accept_runtime_attachment_request(context, row, 0);
                return true;
            }
        }
    }
    tables = context.tables;
    if (tables != nullptr && tables->pair.rows != nullptr)
    {
        row = find_runtime_attachment_pair_request(*tables, key);
        if (row != nullptr && runtime_body_attached_class(context.actions.attached, context.owner_class) ==
            read_u16({row, 0x28}, 0x24))
        {
            if (runtime_body_attached_category(context.actions.attached) == read_u16({row, 0x28}, 0x26))
            {
                accept_runtime_attachment_request(context, row, 0);
                return true;
            }
        }
    }
    tables = context.tables;
    if (tables != nullptr && tables->category.rows != nullptr)
    {
        row = find_runtime_attachment_category_request(*tables, key);
        if (row != nullptr && runtime_attachment_request_matches({row, 0x2C}, context.actions.attached,
            context.secondary, context.owner_class))
        {
            accept_runtime_attachment_request(context, row, 0);
            return true;
        }
    }
    row = find_runtime_attachment_special_request(context.tables, context.actions.attached, key);
    if (row == nullptr)
    {
        return false;
    }
    accept_runtime_attachment_request(context, row, 0);
    return true;
}

// Xbox 0x0001D830
void raise_runtime_body_request_tag(std::span<u8> body, entity_detail::ListHead& live_attachments, u16 bits)
{
    const u16 tag = u16(body_record(body).tag | bits);
    body_record(body).tag = tag;
    auto* first = static_cast<ActorLiveAttachment*>(live_attachments.next); // Body +8 actor +0x8F8.
    if (first != nullptr)
    {
        first->tag = u16(first->tag | bits);
    }
}
}
