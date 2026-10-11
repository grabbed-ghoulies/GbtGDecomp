#include "body_timers.h"
#include "player_goal_cleanup.h"
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
void timer_store(std::span<u8> body, size_t offset, float value)
{
    std::memcpy(body.data() + offset, &value, 4);
}
void notify_timer_latch_change(u8 before, std::span<const u8> body, Actor& owner,
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
}

// Xbox 0x0013B240
void expire_runtime_body_timer_recipient(RuntimeBodyTimerRecipient& recipient)
{
    const std::array<u32, 2> message{5, 0};
    recipient.message(message);
}

// Xbox 0x0001F600
void tick_runtime_body_latch_timer(std::span<u8> body, Actor& owner,
    entity_detail::LatchNode*& first, const RuntimeActorClock& clock)
{
    const float step = runtime_frame_step(clock);
    const float rate = runtime_actor_rate(owner);
    if (read_f32(body, 0x25C) > 0.f)
    {
        const float timer = read_f32(body, 0x25C) - rate * step;
        timer_store(body, 0x25C, timer);
        if (timer <= 0.f)
        {
            timer_store(body, 0x25C, 0.f);
            const u8 before = runtime_body_latch_mask(body);
            for (size_t offset = 0x260; offset != 0x266; ++offset)
            {
                --body[offset];
            }
            notify_timer_latch_change(before, body, owner, first);
        }
    }
}

// Xbox 0x0001FB40
void tick_runtime_body_pulse(std::span<u8> body, Actor& owner,
    entity_detail::LatchNode*& first, const RuntimeActorClock& clock)
{
    tick_runtime_body_latch_timer(body, owner, first, clock);
    const float step = runtime_frame_step(clock);
    const float rate = runtime_actor_rate(owner);
    if (read_f32(body, 0x168) > 0.f)
    {
        const float timer = read_f32(body, 0x168) - rate * step;
        timer_store(body, 0x168, timer);
        if (timer <= 0.f)
        {
            timer_store(body, 0x168, 0.f);
            const u32 zero = 0;
            std::memcpy(body.data() + 0x164, &zero, 4);
            timer_store(body, 0x16C, std::bit_cast<float>(0x3F7D70A4u));
        }
    }
    const float second_step = runtime_frame_step(clock);
    const float second_rate = runtime_actor_rate(owner);
    if (read_f32(body, 0x174) > 0.f)
    {
        const float timer = read_f32(body, 0x174) - second_rate * second_step;
        timer_store(body, 0x174, timer);
        if (timer <= 0.f)
        {
            timer_store(body, 0x174, 0.f);
        }
    }
}

// Xbox 0x00027960
void tick_runtime_body_timers(std::span<u8> body, Actor& owner,
    RuntimeBodyTimerRecipient*& first_recipient, RuntimeBodyTimerRecipient*& second_recipient,
    entity_detail::LatchNode*& first, const RuntimeActorClock& clock)
{
    const float step = runtime_frame_step(clock);
    const float rate = runtime_actor_rate(owner);
    if (read_f32(body, 0x75C) > 0.f)
    {
        const float timer = read_f32(body, 0x75C) - rate * step;
        timer_store(body, 0x75C, timer);
        if (timer <= 0.f)
        {
            timer_store(body, 0x75C, 0.f);
        }
    }
    if (first_recipient != nullptr)
    {
        const float first_step = runtime_frame_step(clock);
        const float first_rate = runtime_actor_rate(owner);
        if (read_f32(body, 0x758) > 0.f)
        {
            const float timer = read_f32(body, 0x758) - first_rate * first_step;
            timer_store(body, 0x758, timer);
            if (timer <= 0.f)
            {
                timer_store(body, 0x758, 0.f);
                if (first_recipient != nullptr)
                {
                    const std::array<u32, 2> message{5, 0};
                    first_recipient->message(message);
                }
            }
        }
    }
    if (body[0x7E4] != 0 && body[0x7E5] == 0)
    {
        const float second_step = runtime_frame_step(clock);
        const float second_rate = runtime_actor_rate(owner);
        if (read_f32(body, 0x7E8) > 0.f)
        {
            const float timer = read_f32(body, 0x7E8) - second_rate * second_step;
            timer_store(body, 0x7E8, timer);
            if (timer <= 0.f)
            {
                timer_store(body, 0x7E8, 0.f);
                if (second_recipient != nullptr)
                {
                    expire_runtime_body_timer_recipient(*second_recipient);
                }
                decrement_runtime_body_latch_mask(body, 0x3F, owner, first);
                body[0x7E4] = 0;
            }
        }
    }
}

// Xbox 0x0012B2A0
bool tick_runtime_body_frame_timer(float& timer, const RuntimeActorClock& clock)
{
    const float step = runtime_frame_step(clock);
    if (timer > 0.f)
    {
        timer -= step;
        if (timer <= 0.f)
        {
            timer = 0.f;
            return true;
        }
    }
    return false;
}
}
