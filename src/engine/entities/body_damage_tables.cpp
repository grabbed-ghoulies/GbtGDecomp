#include "body_damage_tables.h"
#include "body_damage_curve.h"
#include <cstddef>
#include "gbtg/anim.h"
#include "player_mind_events.h"
#include "actor_shape_bounds.h"
#include "body_fields.h"
#include "body_damage_fields.h"
#include <bit>
#include <immintrin.h>
#include <cstring>

namespace gbtg
{
namespace
{
float fold_damage_angle(float angle)
{
    const long double remainder = animation_remainder(angle, std::bit_cast<float>(0x40C90FDBu));
    float result = float(remainder);
    if (remainder < static_cast<long double>(std::bit_cast<float>(0xC0490FDBu)))
    {
        result += std::bit_cast<float>(0x40C90FDBu);
    }
    if (result >= std::bit_cast<float>(0x40490FDBu))
    {
        result -= std::bit_cast<float>(0x40C90FDBu);
    }
    return result;
}
i32 truncate_damage_value(float value)
{
    return _mm_cvtt_ss2si(_mm_set_ss(value));
}
u32 nearest_contact_flags(const entity_detail::SegmentPool& contacts, const Actor& actor, float y)
{
    Vec3 origin = actor_world_position(actor);
    origin.y = y;
    const u8* selected = contacts.begin;
    Vec3 point = origin;
    auto update_point = [&point](const u8* record)
    {
        if (record[0x28] == 1)
        {
            const auto bytes = std::span<const u8>(record, 0x54);
            point = {read_f32(bytes, 0x2C), read_f32(bytes, 0x30), read_f32(bytes, 0x34)};
        }
    };
    auto distance = [&origin, &point]()
    {
        const float x = point.x - origin.x;
        const float y_difference = point.y - origin.y;
        const float z = point.z - origin.z;
        return z * z + y_difference * y_difference + x * x;
    };
    update_point(selected);
    float nearest = distance();
    for (const u8* record = contacts.begin + 0x54; record < contacts.cursor; record += 0x54)
    {
        update_point(record);
        const float candidate = distance();
        if (nearest > candidate)
        {
            selected = record;
            nearest = candidate;
        }
    }
    return read_u32(std::span<const u8>(selected, 0x54), 0x20);
}
}

// Xbox 0x000226C0
bool runtime_body_damage_classified(std::span<const u8> body, const Actor& actor,
    const BodyDamage& damage, const entity_detail::SegmentPool* contacts,
    const Actor* contact_actor, const Actor* source, u32 source_live,
    u32 player_damage, const RuntimeBodyDamageTables& tables,
    std::span<const u8> keys)
{
    if (contacts == nullptr || damage.kind == 0x12)
    {
        return false;
    }
    if ((runtime_actor_physical_shape_flags(actor) & 0x20000) != 0 &&
        runtime_body_damage_amount(body, actor, source, source_live, player_damage,
            damage.key, damage.source_class, tables, keys) == 0)
    {
        return true;
    }
    u32 flags = 0;
    for (const u8* record = contacts->begin; record < contacts->cursor; record += 0x54)
    {
        flags |= read_u32({record, 0x54}, 0x20);
    }
    if ((flags & 0x20000) == 0)
    {
        return false;
    }
    if (i32(contacts->cursor - contacts->begin) / contacts->stride == 1)
    {
        return true;
    }
    return ((nearest_contact_flags(*contacts, *contact_actor, damage.position.y) >> 0x11) &
            1) != 0;
}

// Xbox 0x000244E0
float runtime_body_damage_direction(float facing, const Vec3& direction)
{
    float angle;
    runtime_direction_angle(direction, angle);
    const float opposite = fold_damage_angle(angle + std::bit_cast<float>(0x40490FDBu));
    const float relative = fold_damage_angle(opposite - facing);
    const float absolute = animation_absolute(relative);
    if (absolute >= std::bit_cast<float>(0x3D567750u) && absolute < std::bit_cast<float>(0x3F490FDBu))
    {
        if (!(relative > 0.f))
        {
            return std::bit_cast<float>(0xBF490FDBu);
        }
        return std::bit_cast<float>(0x3F490FDBu);
    }
    return relative;
}

// Xbox 0x000239C0
float runtime_body_damage_push_amount(const Actor& actor, u16 actor_class,
    u32 key, const RuntimeBodyDamageTables& tables, std::span<const u8> keys)
{
    if (key == 0x64)
    {
        return 0.f;
    }
    float result;
    if ((actor.mask_mode == 2 || actor.mask_flag != 0) && actor_class != 0)
    {
        result = runtime_class_damage_value(keys[size_t(key) * 2 + 1], actor_class, tables);
    }
    else
    {
        result = runtime_class_damage_value(keys[size_t(key) * 2], actor.damage_class, tables);
    }
    if (key == 0x67)
    {
        result *= std::bit_cast<float>(0x41200000u);
    }
    return result;
}

// Xbox 0x00023A50
bool runtime_body_damage_parameters(const Actor& actor, u16 actor_class,
    u32 key, const RuntimeBodyDamageTables& tables, std::span<const u8> keys,
    float& first, float& second, float& third, float& fourth)
{
    if (key == 0x64)
    {
        return false;
    }
    if ((actor.mask_mode == 2 || actor.mask_flag != 0) && actor_class != 0)
    {
        first = runtime_class_damage_value(keys[size_t(key) * 2 + 0x11], actor_class, tables);
        second = runtime_class_damage_value(keys[size_t(key) * 2 + 0x21], actor_class, tables);
        third = runtime_class_damage_value(keys[size_t(key) * 2 + 0x31], actor_class, tables);
        fourth = runtime_class_damage_value(0x37, actor_class, tables);
        return true;
    }
    first = runtime_class_damage_value(keys[size_t(key) * 2 + 0x10], actor.damage_class, tables);
    second = runtime_class_damage_value(keys[size_t(key) * 2 + 0x20], actor.damage_class, tables);
    third = runtime_class_damage_value(keys[size_t(key) * 2 + 0x30], actor.damage_class, tables);
    fourth = runtime_class_damage_value(0x18, actor.damage_class, tables);
    return true;
}

// Xbox 0x00022870
bool runtime_body_contact_damage_blocked(const EntityBitBlock& flags,
    const entity_detail::SegmentPool* contacts, const Actor* contact_actor, float contact_y)
{
    if (contacts == nullptr || (flags.bytes[1] & 4) != 0)
    {
        return false;
    }
    u32 combined = 0;
    for (const u8* record = contacts->begin; record < contacts->cursor; record += 0x54)
    {
        combined |= read_u32(std::span<const u8>(record, 0x54), 0x20);
    }
    if ((combined & 0x200000) == 0)
    {
        return false;
    }
    const i32 count = i32(contacts->cursor - contacts->begin) / i32(contacts->stride);
    if (count == 1)
    {
        return true;
    }
    return ((nearest_contact_flags(*contacts, *contact_actor, contact_y) >> 0x15) & 1) != 0;
}
// Xbox 0x001339C0
float interpolate_runtime_damage_value(u8 key, u8 value, const RuntimeBodyDamageTables& tables)
{
    const BodyDamageCurve& curve = retail_row<BodyDamageCurve>(tables.values, key);
    const float first = float(curve.key_first);
    const float last = float(curve.key_last);
    const float upper = curve.value_last;
    const float lower = curve.value_first;
    if (last == first)
    {
        return upper;
    }
    const float result = ((float(value) - first) / (last - first)) * (upper - lower) + lower;
    if (upper > lower)
    {
        if (result > upper)
        {
            return upper;
        }
        if (lower > result)
        {
            return lower;
        }
    }
    else
    {
        if (upper > result)
        {
            return upper;
        }
        if (result > lower)
        {
            return lower;
        }
    }
    return result;
}

// Xbox 0x00133BD0
float runtime_class_damage_value(u8 key, u16 actor_class, const RuntimeBodyDamageTables& tables)
{
    if (actor_class == 0)
    {
        return retail_row<BodyDamageCurve>(tables.values, key).value_last;
    }
    const u32 index = u32(actor_class) * 0x87 + u32(key) - 0x48;
    return interpolate_runtime_damage_value(key, tables.classes[index], tables);
}

// Xbox 0x00028DA0
i32 runtime_actor_damage_value(u8 key, u16 actor_class, const RuntimeBodyDamageTables& tables)
{
    if (actor_class == 0)
    {
        return truncate_damage_value(retail_row<BodyDamageCurve>(tables.values, key).value_last);
    }
    const u32 index = u32(actor_class) * 0x87 + u32(key) - 0x48;
    return truncate_damage_value(interpolate_runtime_damage_value(key, tables.classes[index], tables));
}

// Xbox 0x00028D00
bool runtime_actor_is_player(const Actor& actor)
{
    if (actor.mask_mode == 2)
    {
        return true;
    }
    return actor.mask_flag != 0;
}

// Xbox 0x00028D20
bool runtime_actor_is_live_player(const Actor& actor, u32 live)
{
    if (live != 0 && (actor.mask_mode == 2 || actor.mask_flag != 0))
    {
        return true;
    }
    return false;
}

// Xbox 0x000238E0
u32 runtime_body_damage_amount(std::span<const u8> body, const Actor& actor,
    const Actor* source, u32 source_live, u32 player_damage, u8 key, u16 actor_class,
    const RuntimeBodyDamageTables& tables, std::span<const u8> keys)
{
    if (key == 0x32)
    {
        return 0;
    }
    if ((actor.mask_mode == 2 || actor.mask_flag != 0) && actor_class != 0)
    {
        return u32(truncate_damage_value(runtime_class_damage_value(keys[size_t(key) * 2 + 1], actor_class, tables)));
    }
    u32 amount = u32(runtime_actor_damage_value(keys[size_t(key) * 2], actor.damage_class, tables));
    bool live_player = false;
    if (source != nullptr)
    {
        if (runtime_actor_is_live_player(*source, source_live) && player_damage != 0)
        {
            live_player = true;
        }
    }
    if (amount != 0 && live_player)
    {
        return body_record(body).energy;
    }
    if (key == 0x3F)
    {
        if (amount != 0)
        {
            const i32 half = std::bit_cast<i32>(amount) / 2;
            amount = 1;
            if (half > 1)
            {
                amount = u32(half);
            }
        }
        if (body_record(body).unk_750 != 0)
        {
            amount = 0;
        }
    }
    else if (key == 0x46)
    {
        if (!runtime_actor_is_player(actor))
        {
            return 0;
        }
    }
    return amount;
}

// Xbox 0x00023CB0
i32 runtime_body_damage_pulse_count(const Actor& actor, u16 actor_class, const RuntimeBodyDamageTables& tables)
{
    if ((actor.mask_mode == 2 || actor.mask_flag != 0) && actor_class != 0)
    {
        return truncate_damage_value(runtime_class_damage_value(0x2F, actor_class, tables));
    }
    return runtime_actor_damage_value(0x10, actor.damage_class, tables);
}

// Xbox 0x00023E20
float runtime_body_damage_pause(const Actor& actor, u16 actor_class, const RuntimeBodyDamageTables& tables)
{
    if ((actor.mask_mode == 2 || actor.mask_flag != 0) && actor_class != 0)
    {
        return runtime_class_damage_value(0x2E, actor_class, tables);
    }
    return runtime_class_damage_value(0xF, actor.damage_class, tables);
}

// Xbox 0x00023F70
float runtime_body_damage_source_pause(const Actor& actor, u16 actor_class, const RuntimeBodyDamageTables& tables)
{
    if ((actor.mask_mode == 2 || actor.mask_flag != 0) && actor_class != 0)
    {
        return runtime_class_damage_value(0x30, actor_class, tables);
    }
    return runtime_class_damage_value(0x11, actor.damage_class, tables);
}

// Xbox 0x0001FF60
void reduce_runtime_body_damage_pulse(std::span<u8> body, const Actor& actor, i32 count)
{
    if (count != 0)
    {
        if (actor.mask_mode == 2 || actor.mask_flag != 0)
        {
            const u32 duration = 0x40000000;
            std::memcpy(body.data() + offsetof(BodyRecord, unk_168), &duration, 4);
        }
        if (body_record(body).stagger > 0.f)
        {
            const float value = body_record(body).stagger - std::bit_cast<float>(0x3F7D70A4u) / float(count);
            body_record(body).stagger = value;
            if (value <= 0.f)
            {
                const u32 zero = 0;
                body_record(body).stagger = 0.f;
            }
        }
        if (std::bit_cast<float>(0x3A83126Fu) > body_record(body).stagger)
        {
            body_record(body).stagger = 0.f;
        }
    }
}

// Xbox 0x00022310
u32 apply_runtime_body_damage_health(std::span<u8> body, const Actor& actor,
    const Actor* source, u32 source_live, u32 player_damage,
    const EntityBitBlock& flags, u8 key, u16 actor_class,
    const RuntimeBodyDamageTables& tables, std::span<const u8> keys,
    std::span<const u32, 2> blocked, std::span<const u8> modes, u32 mode)
{
    if ((flags.bytes[0] & 0x80) == 0 &&
        ((actor.mask_mode != 2 && actor.mask_flag == 0) ||
            runtime_player_damage_enabled(actor, blocked, modes, mode) != 0))
    {
        u32 amount = runtime_body_damage_amount(body, actor, source, source_live,
            player_damage, key, actor_class, tables, keys);
        if (source == nullptr || source_live == 0 ||
            (source->mask_mode != 2 && source->mask_flag == 0) || player_damage == 0)
        {
            switch (key)
            {
            case 0x36:
            case 0x39:
            case 0x3A:
            case 0x3B:
            case 0x3C:
            case 0x3D:
            case 0x3E:
            case 0x3F:
            case 0x45:
                if ((flags.bytes[4] & 0x20) != 0 && amount != 0)
                {
                    i32 half = std::bit_cast<i32>(amount) >> 1;
                    if (half <= 1)
                    {
                        half = 1;
                    }
                    amount = u32(half);
                }
                break;
            default:
                break;
            }
        }
        return subtract_runtime_body_damage_health(body, actor, flags, amount, 1, blocked, modes, mode);
    }
    return 0;
}
}


