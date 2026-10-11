#include "body_damage_dispatch.h"

namespace gbtg
{
// Xbox 0x00020DC0
void receive_runtime_body_damage(std::span<u8> body, std::span<u8> damage,
    std::span<u32, 4> result, RuntimeBodyDamageEntry& context)
{
    auto& processor = context.processor;
    result[0] = 0;
    result[2] = 0;
    result[1] = 0;
    result[3] = 0;
    if ((processor.actor.mask_mode != 2 && processor.actor.mask_flag == 0) ||
        runtime_player_damage_enabled(processor.actor, processor.player_blocked,
            processor.player_modes, processor.player_mode) != 0 || read_u32(damage, 0xC) != 0)
    {
        u32 phase = 0;
        const Actor* contact_actor = processor.first_source != nullptr ? processor.first_source->actor : nullptr;
        if (runtime_body_contact_damage_blocked(processor.flags, context.contacts,
                contact_actor, read_f32(damage, 0x1C)) ||
            ((read_u32(damage, 0x10) == 0 || (processor.flags.bytes[3] & 8) != 0) &&
                runtime_body_latches_block(body, processor.actor, read_u32(damage, 4))))
        {
            if ((processor.actor.mask_mode != 2 && processor.actor.mask_flag == 0) ||
                body[0x6A] == 0 || (processor.flags.bytes[7] & 2) != 0)
            {
                notify_runtime_body_blocked_damage(context.game, processor.actor_address,
                    read_u32(damage, 0x38), read_u32(damage, 0x5C), read_u32(damage, 0x60));
                return;
            }
            phase = 1;
        }
        const bool classified = runtime_body_damage_classified(body, processor.actor, damage,
            context.contacts, contact_actor,
            processor.second_source != nullptr ? processor.second_source->actor : nullptr,
            processor.second_source_live, processor.player_damage, processor.tables, processor.amount_keys);
        u32 timer = read_u32(damage, 0x58);
        adjust_runtime_body_damage_kind(damage, processor.attributes, processor.flags);
        if (!classified)
        {
            dispatch_runtime_body_damage(body, damage, result, phase, timer, processor);
        }
        else
        {
            dispatch_runtime_body_classified_damage(body, damage, result, phase, timer, processor);
        }
        if (timer != 0)
        {
            request_runtime_body_hit_timer(body, 5.f, 0, context.timer);
        }
        if (result[1] != 0)
        {
            notify_runtime_body_damage(context.game, processor.actor_address, read_u32(damage, 0x38),
                result[0], result[2], result[3], read_u32(damage, 0x5C), read_u32(damage, 0x60), read_u32(damage, 0x14));
            if (classified)
            {
                notify_runtime_body_classified_damage(context.game, processor.actor_address,
                    read_u32(damage, 0x38), read_u32(damage, 0x5C), read_u32(damage, 0x60));
                return;
            }
        }
    }
}

// Xbox 0x00021220
void dispatch_runtime_body_damage(std::span<u8> body, std::span<const u8> damage,
    std::span<u32, 4> result, u32 phase, u32& timer,
    RuntimeBodyDamageProcessor& context)
{
    switch (damage[0])
    {
    case 1:
        process_runtime_body_reaction_hit(body, damage, result.first<3>(), damage[2], phase, context);
        return;
    case 2:
        process_runtime_body_reaction_hit(body, damage, result.first<3>(), damage[2], phase, context);
        timer = 1;
        return;
    case 3:
    case 0x10:
    case 6:
    case 7:
    case 0xC:
        process_runtime_body_health_knockdown(body, damage, result.first<3>(), phase, damage[2], 1.f, 1.f, context);
        return;
    case 4:
        process_runtime_body_health_knockdown(body, damage, result.first<3>(), phase, damage[2], 1.f, 1.f, context);
        timer = 1;
        return;
    case 5:
        process_runtime_body_health_knockdown(body, damage, result.first<3>(), phase, damage[2], 2.f, 1.75f, context);
        return;
    case 8:
        process_runtime_body_surviving_hit(body, damage, result.first<3>(), phase, context);
        return;
    case 9:
        process_runtime_body_health_knockdown(body, damage, result.first<3>(), phase, damage[2], 1.f, 1.f, context);
        timer = u32(damage[1] == 0x3E);
        return;
    case 0xA:
        process_runtime_body_interrupting_hit(body, damage, result, context);
        return;
    case 0xB:
        process_runtime_body_turning_hit(body, damage, result.first<3>(), phase, context);
        return;
    case 0xD:
        process_runtime_body_direct_reaction(body, damage, result.first<3>(), damage[2], phase, context);
        return;
    case 0xE:
        process_runtime_body_timer_hit(body, damage, phase, context);
        return;
    case 0xF:
        process_runtime_body_amount_hit(body, damage, result.first<3>(), phase, context);
        return;
    case 0x11:
        process_runtime_body_action_hit(body, damage, result.first<3>(), context);
        return;
    case 0x12:
        process_runtime_body_lethal_hit(body, damage, result.first<3>(), context);
        return;
    case 0x13:
        process_runtime_body_health_hit(body, damage, result.first<3>(), context);
        if (result[1] == 0)
        {
            timer = 0;
        }
        return;
    default:
        return;
    }
}

// Xbox 0x00021450
void dispatch_runtime_body_classified_damage(std::span<u8> body,
    std::span<const u8> damage, std::span<u32, 4> result, u32 phase,
    u32& timer, RuntimeBodyDamageProcessor& context)
{
    switch (damage[0])
    {
    case 1:
    case 2:
    case 8:
    case 9:
    case 0xC:
    case 0xD:
    case 0x10:
        break;
    case 3:
    case 4:
    case 6:
    case 7:
        if (runtime_body_bit_set(context.flags, 0x25))
        {
            process_runtime_body_zero_knockdown(body, damage, result.first<3>(), phase, damage[2], 1.f, 1.f, context);
            return;
        }
        break;
    case 5:
        process_runtime_body_zero_knockdown(body, damage, result.first<3>(), phase, damage[2], 2.f, 1.75f, context);
        return;
    case 0xB:
        process_runtime_body_turning_hit(body, damage, result.first<3>(), phase, context);
        return;
    case 0xE:
        process_runtime_body_timer_hit(body, damage, phase, context);
        return;
    case 0xF:
        process_runtime_body_amount_hit(body, damage, result.first<3>(), phase, context);
        return;
    case 0x12:
        process_runtime_body_lethal_hit(body, damage, result.first<3>(), context);
        return;
    case 0x13:
        process_runtime_body_health_hit(body, damage, result.first<3>(), context);
        if (result[1] != 0)
        {
            return;
        }
        timer = 0;
        return;
    default:
        return;
    }
    process_runtime_body_defensive_hit(body, damage, result.first<3>(), phase, context);
}
}
