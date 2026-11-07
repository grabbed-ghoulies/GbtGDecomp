#include "body_action_ids.h"
#include "weapon_fields.h"
#include "player_fields.h"
#include "player_mind_events.h"
#include <bit>
#include "actor_transform.h"
#include "../runtime/name_lookup.h"
#include "body_fields.h"

namespace gbtg
{
// Xbox 0x00075B90
u16 runtime_player_body_primary_id(std::span<const u8> body, const RuntimeBodyActionIds& context,
    const u8* parameters, const EntityBitBlock& flags, const RuntimeScriptLookupTable* table,
    const RuntimePlayerGoalRing* goals, std::span<const u8> address_zero,
    const std::function<u16()>& primary_id, u16 retained_key)
{
    if (body_record(body).energy == 0 && table != nullptr)
    {
        u32 output;
        if (find_runtime_script_table_row(table, 0x6A, output) != nullptr)
        {
            return 0x6A;
        }
    }
    if ((flags.bytes[5] & 0x10) != 0)
    {
        return 0xD9;
    }
    if (goals != nullptr)
    {
        const auto row = goals->tail == goals->capacity ? address_zero :
            goals->rows.subspan(size_t(goals->head) * goals->stride);
        if (read_u16(row, 0) == 7)
        {
            return primary_id(); // Actual body vtable +0x24.
        }
    }
    return runtime_body_primary_parameter_id(context, parameters, flags, table, retained_key);
}

// Xbox 0x00026930
u16 runtime_body_action_name_id(const char* name, const char* const* names)
{
    const i32 index = binary_search_names(0x174, names, name);
    if (index == -1)
    {
        return 0;
    }
    return u16(u32(index) + 1);
}

// Xbox 0x00028810
u32 runtime_player_goal_flag_name_id(const char* name, const char* const* names)
{
    const i32 index = binary_search_names(3, names, name);
    if (index == -1)
    {
        return 0;
    }
    return u32(index) + 1;
}

// Xbox 0x00024C20
bool runtime_body_timer_is_idle(std::span<const u8> body)
{
    return body_record(body).move_holds == 0 && body_record(body).move_hold_time == 0.f;
}

// Xbox 0x00145FF0
u16 classify_runtime_attached_actor(std::span<const u8> attached, u16 tag)
{
    const i16 count = std::bit_cast<i16>(weapon_base(attached).accepted_class_count);
    for (i32 index = 0; index < count; ++index)
    {
        const size_t row = 0x948 + size_t(index) * 0x10;
        if (read_u16(attached, row) == tag)
        {
            return read_u16(attached, row + 4);
        }
    }
    if (count != 0)
    {
        return read_u16(attached, 0x94C);
    }
    return 0;
}

// Xbox 0x00146030
u16 classify_runtime_attached_owner(std::span<const u8> attached, const Actor* owner)
{
    const u16 tag = owner != nullptr ? owner->class_key : 0;
    const i16 count = std::bit_cast<i16>(weapon_base(attached).accepted_class_count);
    for (i32 index = 0; index < count; ++index)
    {
        const size_t row = 0x948 + size_t(index) * 0x10;
        if (read_u16(attached, row) == tag)
        {
            return read_u16(attached, row + 4);
        }
    }
    if (count != 0)
    {
        return read_u16(attached, 0x94C);
    }
    return 0;
}

// Xbox 0x00027620
bool runtime_body_has_action_id(const RuntimeScriptLookupTable* table, u16 key)
{
    if (table == nullptr)
    {
        return false;
    }
    u32 parameters;
    return find_runtime_script_table_row(table, key, parameters) != nullptr;
}

// Xbox 0x001460C0
bool runtime_attached_actor_allows_range(const RuntimeBodyAttachedActor& attached)
{
    if (weapon_base(attached.bytes).unk_93F == 1)
    {
        return true;
    }
    if (weapon_base(attached.bytes).unk_93F != 2)
    {
        return false;
    }
    if (attached.host == nullptr)
    {
        return true;
    }
    return player_record(attached.host->bytes).mode != 2
        && player_record(attached.host->bytes).case_link == 0;
}

namespace
{
void action_pair(const RuntimeBodyActionIds& context, u16& key, u16& fallback,
    const std::array<u16, 7>& keys, const std::array<u16, 7>& fallbacks)
{
    const u8 state = context.attached != nullptr ? weapon_base(context.attached->bytes).unk_93D : 0;
    if (state > 4)
    {
        key = 0;
        fallback = 0;
        return;
    }
    if (state != 1)
    {
        const size_t slot = state == 0 ? 0 : state - 1;
        key = keys[slot];
        fallback = fallbacks[slot];
        return;
    }
    const u16 classification = context.attached == nullptr ? 0
        : classify_runtime_attached_actor(context.attached->bytes, context.tag);
    switch (classification)
    {
    case 1:
    case 2:
    case 3:
        key = keys[3 + classification];
        fallback = fallbacks[3 + classification];
        return;
    case 4:
        key = 0;
        fallback = 0;
        return;
    default:
        return;
    }
}

u16 gated_id(const RuntimeBodyActionIds& context, const RuntimeScriptLookupTable* table,
    const std::array<u16, 7>& ids, bool preserve_address)
{
    u8 state = 0;
    if (context.attached != nullptr)
    {
        state = weapon_base(context.attached->bytes).unk_93D;
        if (state > 4)
        {
            return 0;
        }
    }
    u16 key;
    if (state == 1)
    {
        const u16 classification = context.attached == nullptr ? 0
            : classify_runtime_attached_actor(context.attached->bytes, context.tag);
        switch (classification)
        {
        case 1:
        case 2:
        case 3:
            key = ids[3 + classification];
            break;
        case 4:
            return 0;
        default:
            if (!preserve_address)
            {
                return 0;
            }
            key = u16(context.body_address);
            if (key == 0)
            {
                return 0;
            }
            break;
        }
    }
    else
    {
        key = ids[state == 0 ? 0 : state - 1];
        if (key == 0)
        {
            return 0;
        }
    }
    if (table != nullptr)
    {
        u32 parameters;
        if (find_runtime_script_table_row(table, key, parameters) != nullptr)
        {
            return key;
        }
    }
    return 0;
}

u32 direct_id(const RuntimeBodyActionIds& context, u32 idle, u32 active,
    u32 special, u32 alternate, u32 first, u32 second, u32 third)
{
    u8 state = 0;
    if (context.attached != nullptr)
    {
        state = weapon_base(context.attached->bytes).unk_93D;
        if (state > 4)
        {
            return 0;
        }
    }
    switch (state)
    {
    case 0:
        return idle;
    case 2:
        return active;
    case 3:
        return special;
    case 4:
        return alternate;
    default:
        break;
    }
    const u16 classification = context.attached == nullptr ? 0
        : classify_runtime_attached_actor(context.attached->bytes, context.tag);
    switch (classification)
    {
    case 1:
        return first;
    case 2:
        return second;
    case 3:
        return third;
    case 4:
        return 0;
    default:
        return context.body_address;
    }
}
}

// Xbox 0x00026B00
u32 runtime_body_primary_action_id(const RuntimeBodyActionIds& context)
{
    return direct_id(context, 0x118, 0x119, 0x11B, 0x11A, 0x11D, 0x11E, 0x11C);
}
// Xbox 0x00026BB0
u32 runtime_body_secondary_action_id(const RuntimeBodyActionIds& context)
{
    return direct_id(context, 0x33, 0x34, 0x36, 0x35, 0x38, 0x39, 0x37);
}
// Xbox 0x00027220
u32 runtime_body_contact_action_id(const RuntimeBodyActionIds& context)
{
    return direct_id(context, 0x70, 0x71, 0x73, 0x72, 0x75, 0x76, 0x74);
}
// Xbox 0x00027320
u32 runtime_body_effect_action_id(const RuntimeBodyActionIds& context)
{
    return direct_id(context, 0x56, 0x57, 0x59, 0x58, 0x5B, 0x5C, 0x5A);
}
// Xbox 0x000273D0
u32 runtime_body_damage_action_id(const RuntimeBodyActionIds& context)
{
    return direct_id(context, 0x41, 0x42, 0x44, 0x43, 0x46, 0x47, 0x45);
}
// Xbox 0x00027480
u32 runtime_body_attachment_action_id(const RuntimeBodyActionIds& context)
{
    u8 state = 0;
    if (context.attached != nullptr)
    {
        state = weapon_base(context.attached->bytes).unk_93D;
        if (state > 4)
        {
            return 0;
        }
    }
    if (state == 3)
    {
        return 0x16;
    }
    if (state == 1)
    {
        const u16 classification = context.attached == nullptr ? 0
            : classify_runtime_attached_actor(context.attached->bytes, context.tag);
        if (classification != 1)
        {
            if (classification == 2)
            {
                return 0x18;
            }
            if (classification != 3)
            {
                return context.body_address;
            }
            return 0x17;
        }
    }
    return 0x15;
}
// Xbox 0x00026A70
u32 runtime_body_idle_action_id(const RuntimeBodyActionIds& context)
{
    if (context.attached != nullptr && weapon_base(context.attached->bytes).unk_93D != 0)
    {
        return 0;
    }
    return 0x112;
}
// Xbox 0x000272D0
u32 runtime_body_range_action_id(const RuntimeBodyActionIds& context)
{
    const u8 state = context.attached != nullptr ? weapon_base(context.attached->bytes).unk_93D : 0;
    if (state != 0)
    {
        if (state == 1)
        {
            return (runtime_attached_actor_allows_range(*context.attached) ? 4 : 0) + 0xD6;
        }
        if (state == 4)
        {
            return 0xD7;
        }
    }
    return 0xD6;
}

// Xbox 0x00027060
void runtime_body_primary_action_pair(const RuntimeBodyActionIds& context, u16& key, u16& fallback)
{
    action_pair(context, key, fallback, {0x103, 0x105, 0x107, 0x106, 0x109, 0x10A, 0x108},
        {0x102, 0x10F, 0x111, 0x110, 0x114, 0x115, 0x113});
}
// Xbox 0x00027140
void runtime_body_secondary_action_pair(const RuntimeBodyActionIds& context, u16& key, u16& fallback)
{
    action_pair(context, key, fallback, {0x15D, 0x15E, 0x160, 0x15F, 0x162, 0x163, 0x161},
        {0x15C, 0x167, 0x169, 0x168, 0x16B, 0x16C, 0x16A});
}
// Xbox 0x000269C0
u16 runtime_body_primary_gated_id(const RuntimeBodyActionIds& context, const RuntimeScriptLookupTable* table)
{
    return gated_id(context, table, {0xB6, 0xB8, 0xBA, 0xB9, 0xBC, 0xBD, 0xBB}, false);
}
// Xbox 0x00026DE0
u16 runtime_body_effect_gated_id(const RuntimeBodyActionIds& context, const RuntimeScriptLookupTable* table)
{
    return gated_id(context, table, {0x1C, 0x1D, 0x1F, 0x1E, 0x22, 0x23, 0x21}, true);
}
// Xbox 0x00026EC0
u16 runtime_body_contact_gated_id(const RuntimeBodyActionIds& context, const RuntimeScriptLookupTable* table)
{
    return gated_id(context, table, {0x2D, 0x2E, 0, 0x2F, 0x31, 0x32, 0x30}, true);
}
// Xbox 0x00026F90
u16 runtime_body_damage_gated_id(const RuntimeBodyActionIds& context, const RuntimeScriptLookupTable* table)
{
    return gated_id(context, table, {0xE4, 0xE5, 0xE7, 0xE6, 0xE9, 0xEA, 0xE8}, false);
}

// Xbox 0x00026950
u16 runtime_body_primary_parameter_id(const RuntimeBodyActionIds& context,
    const u8* parameters, const EntityBitBlock& bits,
    const RuntimeScriptLookupTable* table, u16 retained_key)
{
    u16 fallback = u16(context.body_address);
    runtime_body_primary_action_pair(context, retained_key, fallback);
    if (parameters != nullptr && parameters[0x20] != 0 && table != nullptr)
    {
        u32 result;
        if (find_runtime_script_table_row(table, retained_key, result) != nullptr
            && (bits.bytes[7] & 8) == 0)
        {
            return retained_key;
        }
    }
    return fallback;
}

// Xbox 0x00026A90
u16 runtime_body_secondary_parameter_id(const RuntimeBodyActionIds& context,
    const u8* parameters, const EntityBitBlock& bits,
    const RuntimeScriptLookupTable* table, u16 retained_key)
{
    u16 fallback = u16(context.body_address);
    runtime_body_secondary_action_pair(context, retained_key, fallback);
    if (parameters != nullptr && parameters[0x20] != 0 && table != nullptr)
    {
        u32 result;
        if (find_runtime_script_table_row(table, retained_key, result) != nullptr
            && (bits.bytes[7] & 8) == 0)
        {
            return retained_key;
        }
    }
    return fallback;
}

// Xbox 0x00075C30
u16 runtime_player_body_secondary_parameter_id(const RuntimeBodyActionIds& context,
    const u8* parameters, const EntityBitBlock& bits,
    const RuntimeScriptLookupTable* table, u16 retained_key)
{
    if ((bits.bytes[5] & 0x10) != 0)
    {
        return 0xD8;
    }
    return runtime_body_secondary_parameter_id(context, parameters, bits, table, retained_key);
}

// Xbox 0x00076210
u16 runtime_zombie_body_primary_parameter_id(std::span<const u8> body, const RuntimeBodyActionIds& context,
    const u8* parameters, const EntityBitBlock& bits,
    const RuntimeScriptLookupTable* table, u16 retained_key)
{
    if (body_record(body).unk_2D0 != 0 && body_record(body).unk_2D0 == 1 && body_record(body).unk_2D2 != 0)
    {
        return 0x116;
    }
    return runtime_body_primary_parameter_id(context, parameters, bits, table, retained_key);
}

// Xbox 0x00076250
u16 runtime_zombie_body_secondary_parameter_id(std::span<const u8> body, const RuntimeBodyActionIds& context,
    const u8* parameters, const EntityBitBlock& bits,
    const RuntimeScriptLookupTable* table, u16 retained_key)
{
    if (body_record(body).unk_2D0 != 0 && body_record(body).unk_2D0 == 1 && body_record(body).unk_2D2 != 0)
    {
        return 0x16D;
    }
    return runtime_body_secondary_parameter_id(context, parameters, bits, table, retained_key);
}

// Xbox 0x00075A70
u16 runtime_jessie_clyde_body_primary_parameter_id(const RuntimeBodyActionIds& context,
    const u8* parameters, const EntityBitBlock& bits,
    const RuntimeScriptLookupTable* table, u16 retained_key)
{
    if ((bits.bytes[1] & 0x80) != 0)
    {
        return 2;
    }
    return runtime_body_primary_parameter_id(context, parameters, bits, table, retained_key);
}

// Xbox 0x00075AA0
u16 runtime_jessie_clyde_body_secondary_parameter_id(const RuntimeBodyActionIds& context,
    const u8* parameters, const EntityBitBlock& bits,
    const RuntimeScriptLookupTable* table, u16 retained_key)
{
    if ((bits.bytes[1] & 0x80) != 0)
    {
        return 3;
    }
    return runtime_body_secondary_parameter_id(context, parameters, bits, table, retained_key);
}

// Xbox 0x00076100
u16 runtime_worm_body_primary_parameter_id(const RuntimeBodyActionIds& context,
    const u8* parameters, const EntityBitBlock& bits,
    const RuntimeScriptLookupTable* table, u16 retained_key)
{
    if ((bits.bytes[2] & 8) != 0)
    {
        return 0x10C;
    }
    return runtime_body_primary_parameter_id(context, parameters, bits, table, retained_key);
}

// Xbox 0x00076130
u16 runtime_worm_body_secondary_parameter_id(const RuntimeBodyActionIds& context,
    const u8* parameters, const EntityBitBlock& bits,
    const RuntimeScriptLookupTable* table, u16 retained_key)
{
    if ((bits.bytes[2] & 8) != 0)
    {
        return 0x165;
    }
    return runtime_body_secondary_parameter_id(context, parameters, bits, table, retained_key);
}

// Xbox 0x00076160
u16 runtime_worm_body_death_action_key(const EntityBitBlock& bits)
{
    return u16(u16(u16(bits.bytes[2] & 8) | 0x2A4) >> 2);
}

// Xbox 0x00075F30
u16 runtime_vampire_body_primary_parameter_id(const RuntimeBodyActionIds& context,
    const u8* parameters, const EntityBitBlock& bits,
    const RuntimeScriptLookupTable* table, u16 retained_key)
{
    if ((bits.bytes[8] & 1) != 0)
    {
        return 0x29;
    }
    return runtime_body_primary_parameter_id(context, parameters, bits, table, retained_key);
}

// Xbox 0x00075F90
u32 runtime_vampire_body_primary_action_id(const RuntimeBodyActionIds& context, const EntityBitBlock& bits)
{
    if ((bits.bytes[8] & 1) != 0)
    {
        return 0x2A;
    }
    return runtime_body_primary_action_id(context);
}

// Xbox 0x00075F60
u32 runtime_vampire_body_secondary_action_id(const RuntimeBodyActionIds& context, const EntityBitBlock& bits)
{
    if ((bits.bytes[8] & 1) != 0)
    {
        return 0x25;
    }
    return runtime_body_secondary_action_id(context);
}

// Xbox 0x00027500
u16 runtime_body_death_action_key()
{
    return 0xA9;
}

// Xbox 0x00026C60
u16 runtime_body_near_action_id(std::span<const u8> body, Actor& owner,
    RuntimeBodyAttachedActor& attached, const RuntimeScriptLookupTable* table)
{
    const u8 state = weapon_base(attached.bytes).unk_93D;
    if (state == 1)
    {
        const u16 classification = classify_runtime_attached_owner(attached.bytes, &owner);
        switch (classification)
        {
        case 1:
            return 0xC8;
        case 2:
            return 0xC9;
        case 3:
            if (weapon_base(attached.bytes).unk_942 != 0)
            {
                const Vec3 source = actor_world_position(owner);
                const Vec3 target = actor_world_position(*attached.actor);
                const float height = (body_record(body).height_scale * owner.radius_y)
                    * std::bit_cast<float>(0x3F59999Au);
                if (target.y - source.y >= height && runtime_body_has_action_id(table, 0xC7))
                {
                    return 0xC7;
                }
            }
            return 0xC6;
        case 4:
            return 0xC5;
        default:
            break;
        }
    }
    else
    {
        if (state == 2)
        {
            if (weapon_base(attached.bytes).unk_942 != 0)
            {
                const Vec3 source = actor_world_position(owner);
                const Vec3 target = actor_world_position(*attached.actor);
                const float height = (body_record(body).height_scale * owner.radius_y)
                    * std::bit_cast<float>(0x3EB33333u);
                if (target.y - source.y >= height && runtime_body_has_action_id(table, 0xC4))
                {
                    return 0xC4;
                }
            }
            return 0xC3;
        }
        if (state == 3)
        {
            return 0xDC;
        }
    }
    return 0;
}
}
