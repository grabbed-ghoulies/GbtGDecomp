#include "body_damage_notices.h"
#include "actor_live_attachments.h"
#include "../scripts/script_conditions.h"
#include "body_fields.h"
#include "body_damage_fields.h"
#include <array>
#include <bit>
#include <cstring>

namespace gbtg
{
// Xbox 0x00024620
bool runtime_body_death_effect_selected(std::span<const u8> attributes, u32& random)
{
    if (attributes.data() != nullptr && read_u32(attributes, 8) != 0)
    {
        random = random * 0x19660Du + 0x3C6EF35Fu;
        const float value = std::bit_cast<float>((random & 0x7FFFFF) | 0x3F800000u) - 1.f;
        if (read_f32(attributes, 0xC) >= value)
        {
            return true;
        }
    }
    return false;
}

// Xbox 0x00024910
bool notify_runtime_body_death_once(std::span<u8> body, u32 actor_address, Game& game)
{
    if (read_u32(body, 0x284) == 0)
    {
        const std::array<u32, 3> message{0x31, actor_address, get(body, body_field::unk_274)};
        broadcast_actor_pulse_message(reinterpret_cast<const i32*>(message.data()));
        script_queue_event(game, {reinterpret_cast<const u8*>(message.data()), sizeof(message)});
        const u32 one = 1;
        std::memcpy(body.data() + 0x284, &one, 4);
        return true;
    }
    return false;
}

// Xbox 0x00024370
void set_runtime_body_damage_sources(std::span<u8> body, u32 kind,
    u32 amount, u32 first, u32 second, u32 third, u32 increment)
{
    std::memcpy(body.data() + 0x27C, &kind, 4);
    std::memcpy(body.data() + 0x280, &amount, 4);
    std::memcpy(body.data() + 0x26C, &first, 4);
    std::memcpy(body.data() + 0x270, &second, 4);
    std::memcpy(body.data() + 0x274, &third, 4);
    const u32 zero = 0;
    std::memcpy(body.data() + 0x278, &zero, 4);
    if (increment != 0)
    {
        const u16 count = u16(read_u16(body, 0x162) + 1);
        std::memcpy(body.data() + 0x162, &count, 2);
        return;
    }
    const u16 count = 0;
    std::memcpy(body.data() + 0x162, &count, 2);
}

// Xbox 0x00021120
u32 subtract_runtime_body_damage_health(std::span<u8> body, const Actor& actor,
    const EntityBitBlock& flags, u32 amount, u32 double_damage,
    std::span<const u32, 2> blocked, std::span<const u8> modes, u32 mode)
{
    if (actor.mask_mode == 2 || actor.mask_flag != 0)
    {
        if (runtime_player_damage_enabled(actor, blocked, modes, mode) == 0)
        {
            return 0;
        }
    }
    if (double_damage != 0 && (flags.bytes[5] & 0x10) != 0)
    {
        amount += amount;
    }
    if (amount != 0)
    {
        const u32 difference = u32(get(body, body_field::energy)) - amount;
        const u16 health = std::bit_cast<i32>(difference) <= 0 ? 0 : u16(difference);
        std::memcpy(body.data() + 0x94, &health, 2);
    }
    return amount;
}

// Xbox 0x00028D50
u8 runtime_player_damage_enabled(const Actor& actor,
    std::span<const u32, 2> blocked, std::span<const u8> modes, u32 mode)
{
    const u32 index = actor.mask_mode == 2 || actor.mask_flag != 0 ? 1 : 0;
    if (blocked[index] != 0)
    {
        return 0;
    }
    return modes[mode];
}

// Xbox 0x000211D0
void adjust_runtime_body_damage_kind(std::span<u8> damage,
    std::span<const u8> attributes, const EntityBitBlock& flags)
{
    if (attributes.data() != nullptr && attributes[0x1A3] != 0 && (flags.bytes[3] & 2) == 0)
    {
        const u8 kind = get(damage, body_damage_field::kind);
        if (kind != 3)
        {
            if (kind == 4)
            {
                set(damage, body_damage_field::kind, 2);
                set(damage, body_damage_field::key, 0x34);
                set(damage, body_damage_field::push_key, 0x65);
                return;
            }
            if (kind != 7)
            {
                return;
            }
        }
        set(damage, body_damage_field::kind, 1);
        set(damage, body_damage_field::key, 0x33);
        set(damage, body_damage_field::push_key, 0x65);
    }
}

// Xbox 0x00024A80
void notify_runtime_body_blocked_damage(Game& game, u32 source, u32 target, u32 first, u32 second)
{
    const std::array<u32, 5> message{0x70, source, target, first, second};
    broadcast_actor_pulse_message(reinterpret_cast<const i32*>(message.data()));
    script_queue_event(game, {reinterpret_cast<const u8*>(message.data()), sizeof(message)});
}

// Xbox 0x00024AD0
void notify_runtime_body_classified_damage(Game& game, u32 source, u32 target, u32 first, u32 second)
{
    const std::array<u32, 5> message{0x6F, source, target, first, second};
    broadcast_actor_pulse_message(reinterpret_cast<const i32*>(message.data()));
    script_queue_event(game, {reinterpret_cast<const u8*>(message.data()), sizeof(message)});
}

// Xbox 0x00024A00
void notify_runtime_body_damage(Game& game, u32 source, u32 target, u32 first,
    u32 second, u32 third, u32 fourth, u32 fifth, u32 skip_script)
{
    const std::array<u32, 8> message{0x6E, source, target, first, second, third, fourth, fifth};
    broadcast_actor_pulse_message(reinterpret_cast<const i32*>(message.data()));
    if (skip_script == 0)
    {
        script_queue_event(game, {reinterpret_cast<const u8*>(message.data()), sizeof(message)});
    }
}
}
