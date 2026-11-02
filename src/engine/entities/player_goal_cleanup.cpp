#include "player_goal_cleanup.h"
#include "../scripts/script_conditions.h"
#include "actor_live_attachments.h"
#include "entity_heap.h"
#include "player_fields.h"
#include <array>
#include <bit>
#include <cstring>

namespace gbtg
{
// Xbox 0x00135700
bool finish_runtime_player_goal(RuntimePlayerGoalCleanup& context,
    EntityBitBlock*& flags, RuntimeBodyActionSelection& actions,
    const std::function<u16()>& primary_action_key, Game& game, u32 success)
{
    auto body = context.body;
    const u32 zero = 0;
    std::memcpy(context.actor.bytes.data() + 0x98C, &zero, 4);
    if (context.actor.bytes[0x98A] != 0)
    {
        decrement_runtime_body_latches(body, context.body_owner, context.first);
        context.actor.bytes[0x98A] = 0;
    }
    bool result;
    if ((flags->bytes[2] & 2) == 0 && (flags->bytes[4] & 0x10) == 0)
    {
        const u16 key = primary_action_key();
        result = select_runtime_body_action(body, key, actions);
    }
    else
    {
        result = true;
    }
    const std::array<u32, 3> message{0xB7, read_u16(context.actor.bytes, 6), success};
    script_queue_event(game, {reinterpret_cast<const u8*>(message.data()), sizeof(message)});
    return result;
}

namespace
{
void put(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}
void notify_latch_change(u8 before, std::span<const u8> body, Actor& owner,
    entity_detail::LatchNode*& first)
{
    const u8 after = runtime_body_latch_mask(body);
    if (before == 0x3F && after != before)
    {
        Message message;
        message.id = 0x47;
        message.arg = &owner;
        auto* node = first;
        while (node != nullptr)
        {
            auto* recipient = node;
            node = node->next;
            recipient->handle_message(*recipient, message);
        }
    }
}
u16 current_kind(const RuntimePlayerGoalRing* ring, std::span<const u8> address_zero)
{
    if (ring == nullptr)
    {
        return 0;
    }
    if (ring->tail == ring->capacity)
    {
        return read_u16(address_zero, 0);
    }
    return read_u16(ring->rows, size_t(ring->stride) * ring->head);
}
}

// Xbox 0x0001F7A0
u8 runtime_body_latch_mask(std::span<const u8> body)
{
    u8 mask = body[0x260] != 0;
    for (u32 index = 1; index < 6; ++index)
    {
        if (body[0x260 + index] != 0)
        {
            mask = u8(mask + (1u << index));
        }
    }
    return mask;
}

// Xbox 0x0001F2F0
void decrement_runtime_body_latch_mask(std::span<u8> body, u8 mask, Actor& owner,
    entity_detail::LatchNode*& first)
{
    const u8 before = runtime_body_latch_mask(body);
    for (u32 index = 0; index < 6; ++index)
    {
        if ((mask & (1u << index)) != 0)
        {
            body[0x260 + index] = u8(body[0x260 + index] - 1);
        }
    }
    notify_latch_change(before, body, owner, first);
}

// Xbox 0x0001F3F0
void increment_runtime_body_latch_mask(std::span<u8> body, u8 mask, Actor& owner,
    entity_detail::LatchNode*& first)
{
    const u8 before = runtime_body_latch_mask(body);
    for (u32 index = 0; index < 6; ++index)
    {
        if ((mask & (1u << index)) != 0)
        {
            body[0x260 + index] = u8(body[0x260 + index] + 1);
        }
    }
    notify_latch_change(before, body, owner, first);
}

// Xbox 0x0001FC40
void increment_runtime_body_latches(std::span<u8> body, Actor& owner,
    entity_detail::LatchNode*& first)
{
    const u8 before = runtime_body_latch_mask(body);
    for (size_t offset = 0x260; offset < 0x266; ++offset)
    {
        body[offset] = u8(body[offset] + 1);
    }
    notify_latch_change(before, body, owner, first);
}

// Xbox 0x0001FD20
void decrement_runtime_body_latches(std::span<u8> body, Actor& owner,
    entity_detail::LatchNode*& first)
{
    const u8 before = runtime_body_latch_mask(body);
    for (size_t offset = 0x260; offset < 0x266; ++offset)
    {
        body[offset] = u8(body[offset] - 1);
    }
    notify_latch_change(before, body, owner, first);
}

// Xbox 0x0001F4F0, ESI body
void hold_runtime_body_latch_timer(std::span<u8> body, float seconds, Actor& owner,
    entity_detail::LatchNode*& first)
{
    if (read_f32(body, 0x25C) == 0.f) // DAT_0032B100.
    {
        // 0x0001FC40's six raises and their {0x47, actor} notice, inlined.
        increment_runtime_body_latches(body, owner, first);
    }
    std::memcpy(body.data() + 0x25C, &seconds, 4);
}

// Xbox 0x000283D0
bool runtime_goal_requires_latches(u16 kind)
{
    switch (kind)
    {
    case 2:
    case 3:
    case 4:
    case 7:
    case 0xD:
    case 0x10:
    case 0x13:
    case 0x25:
        return false;
    default:
        return true;
    }
}

// Xbox 0x00138030
bool restore_runtime_player_script_overrides(RuntimeBodyAttachedActor& actor,
    RuntimePlayerScriptOverrides& context)
{
    const i16 mode = i16(get(actor.bytes, player_field::mode));
    const bool player = mode == 1;
    if (!player && mode != 5)
    {
        return false;
    }
    u32 count = 0;
    if (context.enabled != 0)
    {
        // Actor +0x930 is the native actor's animation pause, which the frame update counts down.
        if (actor.actor == nullptr)
        {
            throw Error("0x00138030 freezes an actor whose native owner is not bound");
        }
        actor.actor->animation_pause = std::bit_cast<float>(context.value);
        const std::array<u32, 5> message{0x86, context.name, 7, context.argument, 0};
        count = 1;
        actor.message(message);
    }
    if (player && context.player_enabled != 0)
    {
        put(actor.bytes, 0x978, context.player_value);
        const std::array<u32, 5> message{0x86, context.player_name, 0x13, context.player_argument, 0};
        ++count;
        actor.message(message);
    }
    return count > 0;
}

// Xbox 0x00028240
void remove_runtime_player_goal(RuntimePlayerGoalCleanup& context)
{
    if (context.ring != nullptr)
    {
        if (runtime_goal_requires_latches(current_kind(context.ring, context.address_zero)))
        {
            decrement_runtime_body_latches(context.body, context.body_owner, context.first);
        }
        auto* ring = context.ring;
        if (ring->head == ring->tail)
        {
            ring->tail = ring->capacity;
        }
        else
        {
            ring->head = u16((u32(ring->head) + 1) % ring->capacity);
        }
        ring = context.ring;
        if (ring->tail == ring->capacity)
        {
            if (ring != nullptr)
            {
                engine_heap_free(ring);
                context.ring = nullptr;
                put(context.actor.bytes, 0x980, 0);
            }
            put(context.actor.bytes, 0x984, 0);
            const std::array<i32, 2> message{0x79, i32(context.actor.address)};
            broadcast_actor_pulse_message(message.data());
            if (context.actor.bytes[0x98B] != 0)
            {
                context.actor.bytes[0x98B] = 0;
                restore_runtime_player_script_overrides(context.actor, context.overrides);
                return;
            }
        }
        else
        {
            if (runtime_goal_requires_latches(current_kind(context.ring, context.address_zero)))
            {
                increment_runtime_body_latches(context.body, context.body_owner, context.first);
            }
        }
    }
}
}
