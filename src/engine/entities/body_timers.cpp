#include "body_timers.h"
#include <cstddef>
#include "body_fields.h"
#include "player_goal_cleanup.h"
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
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
    if (body_record(body).unk_25C > 0.f)
    {
        const float timer = body_record(body).unk_25C - rate * step;
        body_record(body).unk_25C = timer;
        if (timer <= 0.f)
        {
            body_record(body).unk_25C = 0.f;
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
    if (body_record(body).unk_168 > 0.f)
    {
        const float timer = body_record(body).unk_168 - rate * step;
        body_record(body).unk_168 = timer;
        if (timer <= 0.f)
        {
            body_record(body).unk_168 = 0.f;
            const u32 zero = 0;
            body_record(body).unk_164 = zero;
            body_record(body).stagger = std::bit_cast<float>(0x3F7D70A4u);
        }
    }
    const float second_step = runtime_frame_step(clock);
    const float second_rate = runtime_actor_rate(owner);
    if (body_record(body).unk_174 > 0.f)
    {
        const float timer = body_record(body).unk_174 - second_rate * second_step;
        body_record(body).unk_174 = timer;
        if (timer <= 0.f)
        {
            body_record(body).unk_174 = 0.f;
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
    if (body_record(body).unk_75C > 0.f)
    {
        const float timer = body_record(body).unk_75C - rate * step;
        body_record(body).unk_75C = timer;
        if (timer <= 0.f)
        {
            body_record(body).unk_75C = 0.f;
        }
    }
    if (first_recipient != nullptr)
    {
        const float first_step = runtime_frame_step(clock);
        const float first_rate = runtime_actor_rate(owner);
        if (body_record(body).unk_758 > 0.f)
        {
            const float timer = body_record(body).unk_758 - first_rate * first_step;
            body_record(body).unk_758 = timer;
            if (timer <= 0.f)
            {
                body_record(body).unk_758 = 0.f;
                if (first_recipient != nullptr)
                {
                    const std::array<u32, 2> message{5, 0};
                    first_recipient->message(message);
                }
            }
        }
    }
    if (body_record(body).unk_7E4 != 0 && body_record(body).unk_7E5 == 0)
    {
        const float second_step = runtime_frame_step(clock);
        const float second_rate = runtime_actor_rate(owner);
        if (body_record(body).unk_7E8 > 0.f)
        {
            const float timer = body_record(body).unk_7E8 - second_rate * second_step;
            body_record(body).unk_7E8 = timer;
            if (timer <= 0.f)
            {
                body_record(body).unk_7E8 = 0.f;
                if (second_recipient != nullptr)
                {
                    expire_runtime_body_timer_recipient(*second_recipient);
                }
                decrement_runtime_body_latch_mask(body, 0x3F, owner, first);
                body_record(body).unk_7E4 = 0;
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
