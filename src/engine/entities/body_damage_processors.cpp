#include "body_damage_processors.h"
#include <bit>
#include <cstring>
#include "body_action_ids.h"

namespace gbtg
{
// Xbox 0x000220F0
void process_runtime_body_direct_reaction(std::span<u8> body,
    std::span<const u8> damage, std::span<u32, 3> result, u32 push_key,
    u32 phase, RuntimeBodyDamageProcessor& context)
{
    if ((context.flags.bytes[4] & 0x10) == 0)
    {
        const u32 amount = apply_runtime_body_damage_health(body, context.actor,
            context.second_source != nullptr ? context.second_source->actor : nullptr,
            context.second_source_live, context.player_damage, context.flags,
            damage[1], read_u16(damage, 0x64), context.tables, context.amount_keys,
            context.player_blocked, context.player_modes, context.player_mode);
        if (amount != 0)
        {
            const u16 minimum = u16(!runtime_body_has_death_action(context.actions.actions.table, context.death_actions));
            if (read_u16(body, 0x94) < minimum)
            {
                std::memcpy(body.data() + 0x94, &minimum, 2);
            }
        }
        const i32 pulses = runtime_body_damage_pulse_count(context.actor, read_u16(damage, 0x64), context.tables);
        reduce_runtime_body_damage_pulse(body, context.actor, pulses);
        const u32 count = read_u32(body, 0x164) + 1;
        std::memcpy(body.data() + 0x164, &count, 4);
        push_runtime_body_from_damage(context.actor, context.actor_address, damage,
            context.attributes, push_key, context.first_source, context.second_source,
            context.tables, context.push_keys);
        if (phase == 0)
        {
            auto* source = context.second_source;
            if (source == nullptr)
            {
                source = context.first_source;
            }
            turn_runtime_body_toward_damage(context.actor, *source->actor, context.attributes);
        }
        const u32 kind = damage[0];
        const u32 third = read_u32(damage, 0x3C);
        const u32 second = read_u32(damage, 0x38);
        const u32 first = read_u32(damage, 0x34);
        const u32 zero = 0;
        std::memcpy(body.data() + 0x274, &third, 4);
        std::memcpy(body.data() + 0x27C, &kind, 4);
        std::memcpy(body.data() + 0x26C, &first, 4);
        std::memcpy(body.data() + 0x278, &zero, 4);
        std::memcpy(body.data() + 0x280, &amount, 4);
        std::memcpy(body.data() + 0x270, &second, 4);
        std::memcpy(body.data() + 0x162, &zero, 2);
        const u16 actor_class = read_u16(damage, 0x64);
        float duration = runtime_body_damage_pause(context.actor, actor_class, context.tables);
        float pause = runtime_body_damage_source_pause(context.actor, actor_class, context.tables);
        if (amount == 0)
        {
            duration *= std::bit_cast<float>(0x3F000000u);
            pause *= std::bit_cast<float>(0x3F000000u);
        }
        if (phase == 0)
        {
            const i16 joint = i16(damage[3]);
            const Vec3 direction{read_f32(damage, 0x40), read_f32(damage, 0x44), read_f32(damage, 0x48)};
            const float angle = runtime_body_damage_direction(context.actor.euler.y, direction);
            float factor = float(std::bit_cast<i32>(read_u32(body, 0x164) - 1)) * std::bit_cast<float>(0x3F000000u);
            if (!(std::bit_cast<float>(0x40A00000u) > factor))
            {
                factor = std::bit_cast<float>(0x40A00000u);
            }
            const float timer = factor * duration + duration;
            std::memcpy(body.data() + 0x240, &timer, 4);
            std::memcpy(body.data() + 0x23C, &pause, 4);
            const std::array<u32, 2> message{0x61, 0x7E};
            context.actions.actor_message(message);
            auto& actions = context.actions.actions;
            if (actions.table != nullptr)
            {
                u32 parameters;
                const u8* row = find_runtime_script_table_row(actions.table, 0x7E, parameters);
                if (row != nullptr)
                {
                    select_runtime_body_action_row(body, row, nullptr, actions);
                    if (context.attributes.data() != nullptr)
                    {
                        const AnimationRecord* animation = find_runtime_body_damage_animation(body,
                            context.animation_rows, angle, joint, context.animation_name, actions);
                        if (animation != nullptr)
                        {
                            start_runtime_body_animation_record(*animation, actions);
                        }
                    }
                }
            }
        }
        result[0] = amount;
        result[1] = 1;
    }
}

// Xbox 0x00021920
void process_runtime_body_surviving_hit(std::span<u8> body,
    std::span<const u8> damage, std::span<u32, 3> result, u32 phase,
    RuntimeBodyDamageProcessor& context)
{
    if (read_u32(body, 0x738) != 1 && (context.flags.bytes[3] & 2) == 0)
    {
        const u32 amount = runtime_body_damage_amount(body, context.actor,
            context.second_source != nullptr ? context.second_source->actor : nullptr,
            context.second_source_live, context.player_damage, damage[1], read_u16(damage, 0x64),
            context.tables, context.amount_keys);
        const i32 remaining = std::bit_cast<i32>(u32(read_u16(body, 0x94)) - amount);
        if (remaining > 0)
        {
            process_runtime_body_reaction_hit(body, damage, result, 0x69, phase, context);
            return;
        }
    }
    const u32 amount = apply_runtime_body_damage_health(body, context.actor,
        context.second_source != nullptr ? context.second_source->actor : nullptr,
        context.second_source_live, context.player_damage, context.flags, damage[1],
        read_u16(damage, 0x64), context.tables, context.amount_keys,
        context.player_blocked, context.player_modes, context.player_mode);
    if (phase != 0)
    {
        result[0] = amount;
        result[1] = 1;
        return;
    }
    process_runtime_body_knockdown_hit(body, damage, result, amount, 0x68, 1.f, 1.f, context);
}

// Xbox 0x00021D10
void process_runtime_body_timer_hit(std::span<u8> body,
    std::span<const u8> damage, u32 phase, RuntimeBodyDamageProcessor& context)
{
    if (phase == 0 && read_u16(body, 0x94) != 0)
    {
        const u32 second = read_u32(damage, 0x38);
        const u32 amount = runtime_body_damage_amount(body, context.actor,
            context.second_source != nullptr ? context.second_source->actor : nullptr,
            context.second_source_live, context.player_damage, damage[1], read_u16(damage, 0x64),
            context.tables, context.amount_keys);
        if (amount == 0)
        {
            auto* recipient = context.timer_recipient;
            if (recipient != nullptr && recipient != nullptr && read_u32(body, 0x754) == 0)
            {
                const std::array<u32, 2> message{5, 0};
                recipient->message(message);
                return;
            }
        }
        else
        {
            if ((context.flags.bytes[2] & 2) == 0 && runtime_body_has_action_id(context.actions.actions.table, 0x81))
            {
                const u32 third = read_u32(damage, 0x3C);
                const u32 first = read_u32(damage, 0x34);
                const u32 kind = damage[0];
                const u32 zero = 0;
                std::memcpy(body.data() + 0x274, &third, 4);
                std::memcpy(body.data() + 0x26C, &first, 4);
                std::memcpy(body.data() + 0x27C, &kind, 4);
                std::memcpy(body.data() + 0x280, &amount, 4);
                std::memcpy(body.data() + 0x270, &second, 4);
                std::memcpy(body.data() + 0x278, &zero, 4);
                std::memcpy(body.data() + 0x162, &zero, 2);
                auto& values = context.parameter_scratch;
                runtime_body_damage_parameters(context.actor, read_u16(damage, 0x64),
                    damage[2], context.tables, context.push_keys, values[0], values[1], values[2], values[3]);
                context.random = context.random * 0x19660Du + 0x3C6EF35Fu;
                const float fraction = std::bit_cast<float>((context.random & 0x7FFFFF) | 0x3F800000u) - 1.f;
                const float fourth = (fraction * std::bit_cast<float>(0x3E4CCCD0u) +
                    std::bit_cast<float>(0x3F666666u)) * values[3];
                const Vec3 direction{read_f32(damage, 0x4C), read_f32(damage, 0x50), read_f32(damage, 0x54)};
                set_runtime_body_damage_motion(body, direction, values[0], values[1], values[2], fourth);
                notify_runtime_body_action_key(body, 0x81, context.actions);
            }
        }
    }
}

// Xbox 0x00021EA0
void process_runtime_body_lethal_hit(std::span<u8> body,
    std::span<const u8> damage, std::span<u32, 3> result,
    RuntimeBodyDamageProcessor& context)
{
    const u32 health = read_u16(body, 0x94);
    if ((context.flags.bytes[0] & 0x80) == 0 &&
        ((context.actor.mask_mode != 2 && context.actor.mask_flag == 0) ||
            runtime_player_damage_enabled(context.actor, context.player_blocked, context.player_modes, context.player_mode) != 0)
        && health != 0)
    {
        u32 amount;
        if ((context.actor.mask_mode == 2 || context.actor.mask_flag != 0) &&
            runtime_player_damage_enabled(context.actor, context.player_blocked, context.player_modes, context.player_mode) == 0)
        {
            amount = 0;
        }
        else
        {
            amount = health;
            if ((context.flags.bytes[5] & 0x10) != 0)
            {
                amount = health * 2;
            }
            if (amount != 0)
            {
                const u32 difference = health - amount;
                const u16 remaining = std::bit_cast<i32>(difference) <= 0 ? 0 : u16(difference);
                std::memcpy(body.data() + 0x94, &remaining, 2);
            }
        }
        auto& actions = context.actions.actions;
        u32 parameters;
        if (actions.table == nullptr || find_runtime_script_table_row(actions.table, 0x6B, parameters) == nullptr)
        {
            process_runtime_body_knockdown_hit(body, damage, result, amount, damage[2], 1.f, 1.f, context);
            return;
        }
        const u32 zero = 0;
        const u32 pulse = 0x3F7D70A4;
        std::memcpy(body.data() + 0x164, &zero, 4);
        std::memcpy(body.data() + 0x16C, &pulse, 4);
        set_runtime_body_damage_sources(body, damage[0], amount, read_u32(damage, 0x34),
            read_u32(damage, 0x38), read_u32(damage, 0x3C), read_u32(damage, 8));
        notify_runtime_body_action_key(body, 0x6B, context.actions);
        release_runtime_player_attachment(0, 0, context.actions.attachment);
        result[0] = amount;
        result[2] = 0;
        result[1] = 1;
    }
}

// Xbox 0x00024760
bool select_runtime_body_death_motion(std::span<u8> body, const Vec3& direction,
    float first, float second, float third, float fourth,
    RuntimeBodyDamageProcessor& context)
{
    if (read_u16(body, 0x94) == 0)
    {
        if (runtime_body_death_effect_selected(context.attributes, context.random))
        {
            request_runtime_body_death_effect(1, context.death);
            return true;
        }
    }
    const u16 key = context.death_action();
    auto& actions = context.actions.actions;
    if (actions.table != nullptr)
    {
        u32 parameters;
        if (find_runtime_script_table_row(actions.table, key, parameters) != nullptr)
        {
            const u16 first_count = u16(read_u16(body, 0x160) + 1);
            const u16 second_count = u16(read_u16(body, 0x162) + 1);
            std::memcpy(body.data() + 0x160, &first_count, 2);
            std::memcpy(body.data() + 0x162, &second_count, 2);
            std::memcpy(body.data() + 0x220, &first, 4);
            std::memcpy(body.data() + 0x224, &second, 4);
            std::memcpy(body.data() + 0x228, &third, 4);
            std::memcpy(body.data() + 0x22C, &fourth, 4);
            std::memcpy(body.data() + 0x230, &direction, 12);
            notify_runtime_body_action_key(body, key, context.actions);
            return true;
        }
    }
    return false;
}

// Xbox 0x00022520
void process_runtime_body_knockdown_hit(std::span<u8> body,
    std::span<const u8> damage, std::span<u32, 3> result, u32 amount,
    u32 push_key, float movement_scale, float timer_scale,
    RuntimeBodyDamageProcessor& context)
{
    const u16 key = context.death_action();
    auto& actions = context.actions.actions;
    if (actions.table != nullptr)
    {
        u32 parameters;
        if (find_runtime_script_table_row(actions.table, key, parameters) != nullptr)
        {
            const u32 zero = 0;
            const u32 pulse = 0x3F7D70A4;
            std::memcpy(body.data() + 0x164, &zero, 4);
            std::memcpy(body.data() + 0x16C, &pulse, 4);
            const u32 kind = damage[0];
            const u32 third = read_u32(damage, 0x3C);
            const u32 second = read_u32(damage, 0x38);
            const u32 first = read_u32(damage, 0x34);
            const u32 increment = read_u32(damage, 8);
            std::memcpy(body.data() + 0x274, &third, 4);
            std::memcpy(body.data() + 0x27C, &kind, 4);
            std::memcpy(body.data() + 0x280, &amount, 4);
            std::memcpy(body.data() + 0x26C, &first, 4);
            std::memcpy(body.data() + 0x270, &second, 4);
            std::memcpy(body.data() + 0x278, &zero, 4);
            if (increment == 0)
            {
                std::memcpy(body.data() + 0x162, &zero, 2);
            }
            else
            {
                const u16 count = u16(read_u16(body, 0x162) + 1);
                std::memcpy(body.data() + 0x162, &count, 2);
            }
            auto& values = context.parameter_scratch;
            runtime_body_damage_parameters(context.actor, read_u16(damage, 0x64),
                push_key, context.tables, context.push_keys, values[0], values[1], values[2], values[3]);
            float fourth = values[3];
            if (amount == 0)
            {
                fourth *= std::bit_cast<float>(0x3F000000u);
            }
            context.random = context.random * 0x19660Du + 0x3C6EF35Fu;
            const float fraction = std::bit_cast<float>((context.random & 0x7FFFFF) | 0x3F800000u) - 1.f;
            const float scaled = (fraction * std::bit_cast<float>(0x3E4CCCD0u) +
                std::bit_cast<float>(0x3F666666u)) * fourth;
            const Vec3 direction{read_f32(damage, 0x4C), read_f32(damage, 0x50), read_f32(damage, 0x54)};
            select_runtime_body_death_motion(body, direction, values[0] * movement_scale,
                values[1] * movement_scale, values[2] * timer_scale, scaled, context);
            result[0] = amount;
            result[2] = 1;
            result[1] = 1;
            return;
        }
    }
    process_runtime_body_reaction_hit(body, damage, result, 0x66, 0, context);
}

// Xbox 0x000218A0
void process_runtime_body_health_knockdown(std::span<u8> body,
    std::span<const u8> damage, std::span<u32, 3> result, u32 phase,
    u32 push_key, float movement_scale, float timer_scale,
    RuntimeBodyDamageProcessor& context)
{
    const u32 amount = apply_runtime_body_damage_health(body, context.actor,
        context.second_source != nullptr ? context.second_source->actor : nullptr,
        context.second_source_live, context.player_damage, context.flags, damage[1],
        read_u16(damage, 0x64), context.tables, context.amount_keys,
        context.player_blocked, context.player_modes, context.player_mode);
    if (phase != 0)
    {
        result[0] = amount;
        result[1] = 1;
        return;
    }
    process_runtime_body_knockdown_hit(body, damage, result, amount, push_key,
        movement_scale, timer_scale, context);
}

// Xbox 0x000218E0
void process_runtime_body_zero_knockdown(std::span<u8> body,
    std::span<const u8> damage, std::span<u32, 3> result, u32 phase,
    u32 push_key, float movement_scale, float timer_scale,
    RuntimeBodyDamageProcessor& context)
{
    if (phase != 0)
    {
        result[0] = 0;
        result[1] = 1;
        return;
    }
    process_runtime_body_knockdown_hit(body, damage, result, 0, push_key,
        movement_scale, timer_scale, context);
}

// Xbox 0x000219C0
void process_runtime_body_interrupting_hit(std::span<u8> body,
    std::span<const u8> damage, std::span<u32, 4> result,
    RuntimeBodyDamageProcessor& context)
{
    auto& actions = context.actions.actions;
    if (actions.table != nullptr)
    {
        u32 parameters;
        if (find_runtime_script_table_row(actions.table, 0xDE, parameters) != nullptr)
        {
            const u32 amount = apply_runtime_body_damage_health(body, context.actor,
                context.second_source != nullptr ? context.second_source->actor : nullptr,
                context.second_source_live, context.player_damage, context.flags,
                damage[1], read_u16(damage, 0x64), context.tables, context.amount_keys,
                context.player_blocked, context.player_modes, context.player_mode);
            if (amount != 0)
            {
                const u32 zero = 0;
                const u32 pulse = 0x3F7D70A4;
                std::memcpy(body.data() + 0x164, &zero, 4);
                std::memcpy(body.data() + 0x16C, &pulse, 4);
                push_runtime_body_from_damage(context.actor, context.actor_address, damage,
                    context.attributes, damage[2], context.first_source, context.second_source,
                    context.tables, context.push_keys);
                const u32 kind = damage[0];
                const u32 third = read_u32(damage, 0x3C);
                const u32 second = read_u32(damage, 0x38);
                const u32 first = read_u32(damage, 0x34);
                std::memcpy(body.data() + 0x27C, &kind, 4);
                std::memcpy(body.data() + 0x274, &third, 4);
                std::memcpy(body.data() + 0x280, &amount, 4);
                std::memcpy(body.data() + 0x26C, &first, 4);
                std::memcpy(body.data() + 0x270, &second, 4);
                std::memcpy(body.data() + 0x278, &zero, 4);
                std::memcpy(body.data() + 0x162, &zero, 2);
                notify_runtime_body_action_key(body, 0xDE, context.actions);
                result[0] = amount;
                result[2] = 0;
                result[1] = 1;
                result[3] = 1;
            }
        }
    }
}

// Xbox 0x000215C0
void process_runtime_body_reaction_hit(std::span<u8> body,
    std::span<const u8> damage, std::span<u32, 3> result, u32 push_key,
    u32 phase, RuntimeBodyDamageProcessor& context)
{
    if ((context.flags.bytes[4] & 0x10) == 0)
    {
        const u32 amount = apply_runtime_body_damage_health(body, context.actor,
            context.second_source != nullptr ? context.second_source->actor : nullptr,
            context.second_source_live, context.player_damage, context.flags,
            damage[1], read_u16(damage, 0x64), context.tables, context.amount_keys,
            context.player_blocked, context.player_modes, context.player_mode);
        if (amount != 0)
        {
            const u16 minimum = u16(!runtime_body_has_death_action(context.actions.actions.table, context.death_actions));
            if (read_u16(body, 0x94) < minimum)
            {
                std::memcpy(body.data() + 0x94, &minimum, 2);
            }
        }
        auto& actions = context.actions.actions;
        if (actions.table != nullptr)
        {
            u32 parameters;
            if (find_runtime_script_table_row(actions.table, 0x7E, parameters) != nullptr)
            {
                const u16 actor_class = read_u16(damage, 0x64);
                const i32 pulses = runtime_body_damage_pulse_count(context.actor, actor_class, context.tables);
                reduce_runtime_body_damage_pulse(body, context.actor, pulses);
                const u32 count = read_u32(body, 0x164) + 1;
                std::memcpy(body.data() + 0x164, &count, 4);
                push_runtime_body_from_damage(context.actor, context.actor_address, damage,
                    context.attributes, push_key, context.first_source, context.second_source,
                    context.tables, context.push_keys);
                if (phase == 0)
                {
                    auto* source = context.second_source;
                    if (source == nullptr)
                    {
                        source = context.first_source;
                    }
                    turn_runtime_body_toward_damage(context.actor, *source->actor, context.attributes);
                }
                const u32 kind = damage[0];
                const u32 third = read_u32(damage, 0x3C);
                const u32 second = read_u32(damage, 0x38);
                const u32 first = read_u32(damage, 0x34);
                const u32 zero = 0;
                std::memcpy(body.data() + 0x27C, &kind, 4);
                std::memcpy(body.data() + 0x26C, &first, 4);
                std::memcpy(body.data() + 0x274, &third, 4);
                std::memcpy(body.data() + 0x278, &zero, 4);
                std::memcpy(body.data() + 0x280, &amount, 4);
                std::memcpy(body.data() + 0x270, &second, 4);
                std::memcpy(body.data() + 0x162, &zero, 2);
                const u16 pause_class = read_u16(damage, 0x64);
                float duration = runtime_body_damage_pause(context.actor, pause_class, context.tables);
                float pause = runtime_body_damage_source_pause(context.actor, pause_class, context.tables);
                if (amount == 0)
                {
                    duration *= std::bit_cast<float>(0x3F000000u);
                    pause *= std::bit_cast<float>(0x3F000000u);
                }
                if (phase == 0)
                {
                    const i16 joint = i16(damage[3]);
                    const Vec3 direction{read_f32(damage, 0x40), read_f32(damage, 0x44), read_f32(damage, 0x48)};
                    const float angle = runtime_body_damage_direction(context.actor.euler.y, direction);
                    select_runtime_body_damage_reaction(body, 0x7E, duration, pause, angle,
                        joint, 1, context.attributes, context.animation_rows, context.animation_name, context.actions);
                }
                result[1] = 1;
            }
        }
        result[0] = amount;
    }
}

// Xbox 0x00024680
bool runtime_body_has_death_action(const RuntimeScriptLookupTable* table,
    std::span<const u16, 3> keys)
{
    u32 index = 0;
    do
    {
        if (table != nullptr)
        {
            u32 parameters;
            if (find_runtime_script_table_row(table, keys[index], parameters) != nullptr)
            {
                return true;
            }
        }
        ++index;
    } while (index < 3);
    return false;
}

// Xbox 0x00021B70
void process_runtime_body_turning_hit(std::span<u8> body,
    std::span<const u8> damage, std::span<u32, 3> result, u32 phase,
    RuntimeBodyDamageProcessor& context)
{
    auto& actions = context.actions.actions;
    if (phase == 0 && actions.table != nullptr)
    {
        u32 parameters;
        const u8* row = find_runtime_script_table_row(actions.table, 0x10, parameters);
        if (row != nullptr && (context.flags.bytes[2] & 1) == 0)
        {
            const u32 zero = 0;
            const u32 pulse = 0x3F7D70A4;
            std::memcpy(body.data() + 0x164, &zero, 4);
            std::memcpy(body.data() + 0x16C, &pulse, 4);
            push_runtime_body_from_damage(context.actor, context.actor_address, damage,
                context.attributes, damage[2], context.first_source, context.second_source,
                context.tables, context.push_keys);
            auto* source = context.second_source;
            if (source == nullptr)
            {
                source = context.first_source;
            }
            turn_runtime_body_toward_damage(context.actor, *source->actor, context.attributes);
            const u32 third = read_u32(damage, 0x3C);
            const u32 second = read_u32(damage, 0x38);
            const u32 first = read_u32(damage, 0x34);
            const u32 kind = damage[0];
            std::memcpy(body.data() + 0x274, &third, 4);
            std::memcpy(body.data() + 0x27C, &kind, 4);
            std::memcpy(body.data() + 0x280, &zero, 4);
            std::memcpy(body.data() + 0x26C, &first, 4);
            std::memcpy(body.data() + 0x270, &second, 4);
            std::memcpy(body.data() + 0x278, &zero, 4);
            std::memcpy(body.data() + 0x162, &zero, 2);
            notify_runtime_body_action_key(body, 0x10, context.actions);
            result[1] = 1;
        }
    }
}

// Xbox 0x00021C50
void process_runtime_body_amount_hit(std::span<u8> body,
    std::span<const u8> damage, std::span<u32, 3> result, u32 phase,
    RuntimeBodyDamageProcessor& context)
{
    if (phase == 0)
    {
        const u32 second = read_u32(damage, 0x38);
        const u32 amount = runtime_body_damage_amount(body, context.actor,
            context.second_source != nullptr ? context.second_source->actor : nullptr,
            context.second_source_live, context.player_damage, damage[1], read_u16(damage, 0x64),
            context.tables, context.amount_keys);
        auto& actions = context.actions.actions;
        if (amount != 0 && (context.flags.bytes[2] & 2) == 0 && actions.table != nullptr)
        {
            u32 parameters;
            if (find_runtime_script_table_row(actions.table, 0x81, parameters) != nullptr)
            {
                const u32 third = read_u32(damage, 0x3C);
                const u32 first = read_u32(damage, 0x34);
                const u32 kind = damage[0];
                const u32 zero = 0;
                std::memcpy(body.data() + 0x274, &third, 4);
                std::memcpy(body.data() + 0x26C, &first, 4);
                std::memcpy(body.data() + 0x280, &amount, 4);
                std::memcpy(body.data() + 0x278, &zero, 4);
                std::memcpy(body.data() + 0x27C, &kind, 4);
                std::memcpy(body.data() + 0x270, &second, 4);
                std::memcpy(body.data() + 0x162, &zero, 2);
                notify_runtime_body_action_key(body, 0x81, context.actions);
                result[1] = 1;
            }
        }
    }
}

// Xbox 0x00021760
void process_runtime_body_defensive_hit(std::span<u8> body,
    std::span<const u8> damage, std::span<u32, 3> result, u32 phase,
    RuntimeBodyDamageProcessor& context)
{
    if ((context.flags.bytes[4] & 0x10) == 0)
    {
        const u32 count = read_u32(body, 0x164) + 1;
        std::memcpy(body.data() + 0x164, &count, 4);
        push_runtime_body_from_damage(context.actor, context.actor_address, damage,
            context.attributes, 0x66, context.first_source, context.second_source,
            context.tables, context.push_keys);
        const u32 first = read_u32(damage, 0x34);
        const u32 second = read_u32(damage, 0x38);
        const u32 third = read_u32(damage, 0x3C);
        const u32 zero = 0;
        const u32 one = 1;
        std::memcpy(body.data() + 0x274, &third, 4);
        std::memcpy(body.data() + 0x27C, &one, 4);
        std::memcpy(body.data() + 0x280, &zero, 4);
        std::memcpy(body.data() + 0x26C, &first, 4);
        std::memcpy(body.data() + 0x270, &second, 4);
        std::memcpy(body.data() + 0x278, &zero, 4);
        std::memcpy(body.data() + 0x162, &zero, 2);
        if (phase == 0 && (context.flags.bytes[1] & 2) == 0)
        {
            float factor = float(std::bit_cast<i32>(read_u32(body, 0x164) - 1)) * std::bit_cast<float>(0x3F000000u);
            if (!(std::bit_cast<float>(0x40A00000u) > factor))
            {
                factor = std::bit_cast<float>(0x40A00000u);
            }
            const float timer = factor * 0.f;
            std::memcpy(body.data() + 0x240, &timer, 4);
            std::memcpy(body.data() + 0x23C, &zero, 4);
            const std::array<u32, 2> message{0x61, 0x7F};
            context.actions.actor_message(message);
            auto& actions = context.actions.actions;
            if (actions.table != nullptr)
            {
                u32 parameters;
                const u8* row = find_runtime_script_table_row(actions.table, 0x7F, parameters);
                if (row != nullptr)
                {
                    select_runtime_body_action_row(body, row, nullptr, actions);
                }
            }
            select_runtime_body_animation("hitdefensive", actions);
        }
        result[1] = 1;
    }
}

// Xbox 0x00021A90
void process_runtime_body_action_hit(std::span<u8> body,
    std::span<const u8> damage, std::span<u32, 3> result,
    RuntimeBodyDamageProcessor& context)
{
    if (read_u16(body, 0x30) == 0 && read_f32(body, 0x34) == 0.f && body[0x6A] == 0)
    {
        const u16 key = context.defensive_action();
        auto& actions = context.actions.actions;
        if (actions.table != nullptr)
        {
            u32 parameters;
            if (find_runtime_script_table_row(actions.table, key, parameters) != nullptr)
            {
                const u32 zero = 0;
                const u32 pulse = 0x3F7D70A4;
                std::memcpy(body.data() + 0x164, &zero, 4);
                std::memcpy(body.data() + 0x16C, &pulse, 4);
                const u32 first = read_u32(damage, 0x34);
                const u32 second = read_u32(damage, 0x38);
                const u32 third = read_u32(damage, 0x3C);
                const u32 kind = damage[0];
                std::memcpy(body.data() + 0x27C, &kind, 4);
                std::memcpy(body.data() + 0x280, &zero, 4);
                std::memcpy(body.data() + 0x278, &zero, 4);
                std::memcpy(body.data() + 0x26C, &first, 4);
                std::memcpy(body.data() + 0x270, &second, 4);
                std::memcpy(body.data() + 0x274, &third, 4);
                std::memcpy(body.data() + 0x162, &zero, 2);
                notify_runtime_body_action_key(body, key, context.actions);
                result[0] = 0;
                result[2] = 0;
                result[1] = 1;
            }
        }
    }
}

// Xbox 0x00022010
void process_runtime_body_health_hit(std::span<u8> body,
    std::span<const u8> damage, std::span<u32, 3> result,
    RuntimeBodyDamageProcessor& context)
{
    if ((context.flags.bytes[0] & 0x80) == 0 &&
        ((context.actor.mask_mode != 2 && context.actor.mask_flag == 0) ||
            runtime_player_damage_enabled(context.actor, context.player_blocked,
                context.player_modes, context.player_mode) != 0))
    {
        auto& actions = context.actions.actions;
        if (actions.table != nullptr)
        {
            u32 parameters;
            if (find_runtime_script_table_row(actions.table, 0x80, parameters) != nullptr)
            {
                const u32 amount = subtract_runtime_body_damage_health(body, context.actor,
                    context.flags, read_u16(body, 0x94), 1, context.player_blocked,
                    context.player_modes, context.player_mode);
                const u32 zero = 0;
                const u32 pulse = 0x3F7D70A4;
                std::memcpy(body.data() + 0x164, &zero, 4);
                std::memcpy(body.data() + 0x16C, &pulse, 4);
                set_runtime_body_damage_sources(body, damage[0], amount,
                    read_u32(damage, 0x34), read_u32(damage, 0x38), read_u32(damage, 0x3C), read_u32(damage, 8));
                notify_runtime_body_action_key(body, 0x80, context.actions);
                result[0] = amount;
                result[2] = 0;
                result[1] = 1;
            }
        }
    }
}
}

