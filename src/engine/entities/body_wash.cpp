#include "body_wash.h"
#include <cstddef>
#include "body_timers.h"
#include "../audio/listener_settings.h"
#include "gbtg/anim.h"
#include "body_fields.h"
#include "player_fields.h"
#include <bit>
#include <cstring>

namespace gbtg
{
// Xbox 0x00104DF0
bool tick_runtime_body_oscillator(BodyOscillator& oscillator, float elapsed,
    const std::function<void(BodyOscillator&)>& callback)
{
    float timer = oscillator.restart_timer - elapsed;
    const float period = 1.f / oscillator.rate;
    oscillator.restart_timer = timer;
    bool restarted = false;
    if (timer <= 0.f)
    {
        oscillator.restart_timer = timer + period;
        oscillator.time_left = oscillator.duration;
        restarted = true;
        if (callback)
        {
            callback(oscillator);
        }
    }
    if (oscillator.time_left > 0.f)
    {
        const float angle = float(static_cast<long double>(oscillator.time_left)
            / oscillator.duration * std::bit_cast<float>(0x40490FDBu));
        const float wave = float(retail_sin(angle));
        retail_cos(angle);
        const float base = oscillator.base;
        oscillator.value = float(static_cast<long double>(wave) * oscillator.amplitude * base + base);
    }
    else
    {
        oscillator.value = oscillator.base;
    }
    timer = oscillator.time_left - elapsed;
    oscillator.time_left = timer;
    if (0.f > timer)
    {
        oscillator.time_left = 0.f;
    }
    return restarted;
}

// Xbox 0x0001E270
float runtime_body_health_fraction(std::span<const u8> body)
{
    if (body_record(body).max_energy == 0)
    {
        return 1.f;
    }
    return float(body_record(body).energy) / float(body_record(body).max_energy);
}

// Xbox 0x0001E770
bool runtime_body_actor_requires_wash(std::span<const u8> actor, const u8* bound)
{
    if (player_record(actor).goal_volume != 0)
    {
        return false;
    }
    if (!(player_record(actor).countdown_56 > 0.f) && player_record(actor).provoked == 0)
    {
        return false;
    }
    if (bound != nullptr && read_u32({bound, 0xBC}, 0xB8) != 0)
    {
        return false;
    }
    return true;
}

// Xbox 0x0001E300
void update_runtime_body_wash_health(std::span<u8> body)
{
    BodyRecord& record = body_record(body);
    if (record.max_energy != 0)
    {
        const float fraction = float(record.energy) / float(record.max_energy);
        if (0.25f > fraction)
        {
            record.overlay_pulse.rate = 4.f;
            return;
        }
        if (0.5f > fraction)
        {
            record.overlay_pulse.rate = 3.f;
            return;
        }
        if (0.75f > fraction)
        {
            record.overlay_pulse.rate = 2.f;
            return;
        }
    }
    record.overlay_pulse.rate = 1.f;
}

// Xbox 0x0001E380
void tick_runtime_body_wash_oscillator(std::span<u8> body, const RuntimeActorClock& clock)
{
    BodyRecord& record = body_record(body);
    if (record.energy == 0)
    {
        record.overlay_pulse.base = 2.625f; // DAT_0032B2DC.
        record.overlay_pulse.amplitude = 0.f;
    }
    tick_runtime_body_oscillator(record.overlay_pulse, runtime_frame_step(clock), {});
}

// Xbox 0x0001E6C0
void tick_runtime_body_wash_flashes(std::span<u8> body, const RuntimeActorClock& clock)
{
    BodyRecord& record = body_record(body);
    if (record.energy == 0)
    {
        const float timer = record.overlay_flash_timer - runtime_frame_step(clock);
        record.overlay_flash_timer = timer;
        if (timer <= 0.f)
        {
            record.overlay_flash_timer = timer + 0.1f;
            record.overlay_flash ^= 1;
        }
    }
    if (record.overlay_second_flashes != 0)
    {
        const float timer = record.overlay_second_flash_timer - runtime_frame_step(clock);
        record.overlay_second_flash_timer = timer;
        if (timer <= 0.f)
        {
            record.overlay_second_flash_timer = timer + 0.75f;
            record.overlay_second_flash ^= 1;
        }
    }
    else
    {
        record.overlay_second_flash = 0;
    }
}

// Xbox 0x0001DD00
void tick_runtime_body_wash(std::span<u8> body, std::span<const u8> actor,
    const u8* bound, const RuntimeActorClock& clock)
{
    BodyRecord& record = body_record(body);
    if (record.overlay_enabled == 0)
    {
        return;
    }
    switch (record.overlay_state)
    {
    case 0:
    {
        bool required = false;
        if (record.overlay_held_off == 0)
        {
            if (record.overlay_hold_time > 0.f)
            {
                required = true;
            }
            else
            {
                required = 0.25f > runtime_body_health_fraction(body);
            }
        }
        if (runtime_body_actor_requires_wash(actor, bound) || required)
        {
            record.overlay_pulse.value = 1.75f;
            record.overlay_pulse.base = 1.75f;
            record.overlay_pulse.amplitude = 0.5f;
            record.overlay_pulse.duration = 0.2f;
            record.overlay_pulse.rate = 3.f;
            record.overlay_pulse.time_left = 0.f;
            record.overlay_pulse.restart_timer = 0.f;
            record.overlay_state = 1;
            record.overlay_fade_time = 0.4f;
            record.overlay_alpha = 0;
            record.overlay_flash = 1;
            update_runtime_body_wash_health(body);
        }
        break;
    }
    case 1:
    {
        tick_runtime_body_wash_oscillator(body, clock);
        tick_runtime_body_wash_flashes(body, clock);
        update_runtime_body_wash_health(body);
        float opacity = 128.f - (record.overlay_fade_time * 2.5f) * 128.f;
        if (0.f > opacity)
        {
            opacity = 0.f;
        }
        else if (opacity > 128.f)
        {
            opacity = 128.f;
        }
        record.overlay_alpha = u8(truncate_runtime_float(opacity));
        float timer = record.overlay_fade_time;
        if (tick_runtime_body_frame_timer(timer, clock))
        {
            record.overlay_state = 2;
        }
        record.overlay_fade_time = timer;
        break;
    }
    case 2:
    {
        tick_runtime_body_wash_oscillator(body, clock);
        const float fraction = runtime_body_health_fraction(body);
        float hold = record.overlay_hold_time;
        tick_runtime_body_frame_timer(hold, clock);
        record.overlay_hold_time = hold;
        tick_runtime_body_wash_flashes(body, clock);
        update_runtime_body_wash_health(body);
        record.overlay_alpha = 0x80;
        bool recovered = false;
        const float recovery_timer = record.overlay_hold_time;
        if (recovery_timer == 0.f && fraction > std::bit_cast<float>(0x3EAA7EFAu))
        {
            recovered = true;
        }
        else if (record.overlay_held_off != 0)
        {
            recovered = true;
        }
        if (!runtime_body_actor_requires_wash(actor, bound) && recovered)
        {
            record.overlay_state = 3;
            record.overlay_fade_time = 0.5f;
        }
        break;
    }
    case 3:
    {
        tick_runtime_body_wash_oscillator(body, clock);
        update_runtime_body_wash_health(body);
        float opacity = (record.overlay_fade_time * 2.f) * 128.f;
        if (opacity > 128.f)
        {
            opacity = 128.f;
        }
        else if (0.f > opacity)
        {
            opacity = 0.f;
        }
        record.overlay_alpha = u8(truncate_runtime_float(opacity));
        float timer = record.overlay_fade_time;
        if (tick_runtime_body_frame_timer(timer, clock))
        {
            record.overlay_state = 0;
            record.overlay_alpha = 0;
        }
        record.overlay_fade_time = timer;
        break;
    }
    default:
        break;
    }
}
}
