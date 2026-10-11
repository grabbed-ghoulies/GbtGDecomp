#pragma once
#include "actor_clock.h"
#include "entity_types.h"
#include <functional>

namespace gbtg
{
struct RuntimeBodyTimerRecipient
{
    std::function<void(std::span<const u32>)> message; // Actual vtable +0xC.
};
void expire_runtime_body_timer_recipient(RuntimeBodyTimerRecipient& recipient); // Xbox 0x0013B240
void tick_runtime_body_latch_timer(std::span<u8> body, Actor& owner,
    entity_detail::LatchNode*& first, const RuntimeActorClock& clock); // Xbox 0x0001F600
void tick_runtime_body_pulse(std::span<u8> body, Actor& owner,
    entity_detail::LatchNode*& first, const RuntimeActorClock& clock); // Xbox 0x0001FB40
void tick_runtime_body_timers(std::span<u8> body, Actor& owner,
    RuntimeBodyTimerRecipient*& first_recipient, RuntimeBodyTimerRecipient*& second_recipient,
    entity_detail::LatchNode*& first, const RuntimeActorClock& clock); // Xbox 0x00027960
bool tick_runtime_body_frame_timer(float& timer, const RuntimeActorClock& clock); // Xbox 0x0012B2A0
}
