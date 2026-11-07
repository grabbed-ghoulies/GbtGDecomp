#pragma once
#include "body_fields.h"
#include "actor_clock.h"
#include <functional>

namespace gbtg
{
bool tick_runtime_body_oscillator(BodyOscillator& oscillator, float elapsed,
    const std::function<void(BodyOscillator&)>& callback); // Xbox 0x00104DF0
float runtime_body_health_fraction(std::span<const u8> body); // Xbox 0x0001E270
bool runtime_body_actor_requires_wash(std::span<const u8> actor,
    const u8* bound); // Xbox 0x0001E770
void update_runtime_body_wash_health(std::span<u8> body); // Xbox 0x0001E300
void tick_runtime_body_wash_oscillator(std::span<u8> body,
    const RuntimeActorClock& clock); // Xbox 0x0001E380
void tick_runtime_body_wash_flashes(std::span<u8> body,
    const RuntimeActorClock& clock); // Xbox 0x0001E6C0
void tick_runtime_body_wash(std::span<u8> body, std::span<const u8> actor,
    const u8* bound, const RuntimeActorClock& clock); // Xbox 0x0001DD00
}
