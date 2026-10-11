#include "body_damage_animation.h"
#include "body_message_helpers.h"
#include <bit>
#include <cstdio>
#include <cstring>

namespace gbtg
{
namespace
{
float fold_animation_angle(float angle)
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
const char* attached_animation_variant(RuntimeBodyActionSelection& context)
{
    if (context.attached == nullptr || context.attached->address == 0xFFFFF250u)
    {
        return nullptr;
    }
    const char* variant = reinterpret_cast<const char*>(context.attached->bytes.data() + 0xDB0);
    if (*variant == '\0')
    {
        return nullptr;
    }
    return variant;
}
}

// Xbox 0x00105C10
void select_runtime_damage_angle(RuntimeDamageAngleSelection& selection, float angle)
{
    if (selection.count == 0)
    {
        selection.angle = fold_animation_angle(angle);
        selection.index = selection.count;
    }
    else
    {
        const float candidate = fold_animation_angle(float(static_cast<long double>(angle) - selection.reference));
        const float previous = fold_animation_angle(float(static_cast<long double>(selection.angle) - selection.reference));
        const float previous_distance = animation_absolute(previous);
        const float candidate_distance = animation_absolute(candidate);
        if (previous_distance > candidate_distance)
        {
            selection.angle = fold_animation_angle(angle);
            selection.index = selection.count;
        }
    }
    ++selection.count;
}

// Xbox 0x00020B50
const char* runtime_damage_animation_direction(const RuntimeDamageAnimationRows& rows, float angle)
{
    RuntimeDamageAngleSelection selection{fold_animation_angle(angle), 0, 0, 0.f};
    i32 index = 0;
    if (rows.count > 0)
    {
        do
        {
            select_runtime_damage_angle(selection, rows.rows[size_t(index)].angle * std::bit_cast<float>(0x3C8EFA35u));
            ++index;
        } while (index < rows.count);
    }
    return rows.rows[selection.index].name;
}

// Xbox 0x00020C10
const char* runtime_damage_animation_joint(i16 joint)
{
    switch (joint)
    {
    case 0:
        return "head";
    case 1:
    case 0x12:
        return "torso";
    case 2:
    case 8:
    case 10:
    case 0xB:
        return "leftarm";
    case 3:
    case 9:
    case 0xC:
    case 0xD:
        return "rightarm";
    case 4:
    case 6:
    case 0xE:
    case 0xF:
        return "leftleg";
    case 5:
    case 7:
    case 0x10:
    case 0x11:
        return "rightleg";
    default:
        return "";
    }
}

// Xbox 0x00020C80
const AnimationRecord* find_runtime_body_damage_animation(std::span<const u8> body,
    const RuntimeDamageAnimationRows& rows, float angle, i16 joint,
    std::span<char> buffer, RuntimeBodyActionSelection& context)
{
    const char* direction = runtime_damage_animation_direction(rows, angle);
    const char* part = runtime_damage_animation_joint(joint);
    const char* body_variant = reinterpret_cast<const char*>(body.data() + 0x288);
    if (context.body_address != 0xFFFFFD78u && *body_variant != '\0')
    {
        std::sprintf(buffer.data(), "%s_%s_%s", direction, body_variant, part);
        const char* variant = attached_animation_variant(context);
        const AnimationRecord* record = find_actor_animation(context.animation.table, buffer.data(), variant, context.random);
        if (record != nullptr)
        {
            return record;
        }
        if (variant != nullptr)
        {
            record = find_actor_animation(context.animation.table, buffer.data(), nullptr, context.random);
            if (record != nullptr)
            {
                return record;
            }
        }
        std::sprintf(buffer.data(), "%s_%s", direction, body_variant);
        record = find_runtime_body_animation(buffer.data(), context);
        if (record != nullptr)
        {
            return record;
        }
    }
    std::sprintf(buffer.data(), "%s_%s", direction, part);
    const char* variant = attached_animation_variant(context);
    const AnimationRecord* record = find_actor_animation(context.animation.table, buffer.data(), variant, context.random);
    if (record == nullptr && (variant == nullptr ||
        (record = find_actor_animation(context.animation.table, buffer.data(), nullptr, context.random)) == nullptr))
    {
        record = find_runtime_body_animation(buffer.data(), context);
    }
    return record;
}

// Xbox 0x00024970
void select_runtime_body_damage_reaction(std::span<u8> body, u16 key,
    float duration, float pause, float angle, i16 joint, u32 animate,
    std::span<const u8> attributes, const RuntimeDamageAnimationRows& rows,
    std::span<char> buffer, RuntimeBodyActionMessages& context)
{
    const i32 count = std::bit_cast<i32>(read_u32(body, 0x164) - 1);
    float factor = float(count) * std::bit_cast<float>(0x3F000000u);
    if (!(std::bit_cast<float>(0x40A00000u) > factor))
    {
        factor = std::bit_cast<float>(0x40A00000u);
    }
    const float timer = factor * duration + duration;
    std::memcpy(body.data() + 0x240, &timer, 4);
    std::memcpy(body.data() + 0x23C, &pause, 4);
    if (notify_runtime_body_action_key(body, key, context) && attributes.data() != nullptr && animate != 0)
    {
        const AnimationRecord* record = find_runtime_body_damage_animation(body, rows, angle, joint, buffer, context.actions);
        if (record != nullptr)
        {
            start_runtime_body_animation_record(*record, context.actions);
        }
    }
}
}
