#include "body_damage_processors.h"
#include <bit>
#include <cstring>

namespace gbtg
{
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

