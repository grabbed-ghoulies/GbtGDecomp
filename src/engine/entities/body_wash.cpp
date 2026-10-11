#include "body_wash.h"
#include "body_timers.h"
#include "../audio/listener_settings.h"
#include "gbtg/anim.h"
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
void wash_store(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}
void wash_tick_timer(std::span<u8> bytes, size_t offset, const RuntimeActorClock& clock)
{
    float timer = read_f32(bytes, offset);
    tick_runtime_body_frame_timer(timer, clock);
    wash_store(bytes, offset, timer);
}
}

// Xbox 0x00104DF0
bool tick_runtime_body_oscillator(std::span<u8> oscillator, float elapsed,
    const std::function<void(std::span<u8>)>& callback)
{
    float timer = read_f32(oscillator, 0x14) - elapsed;
    const float period = 1.f / read_f32(oscillator, 0x18);
    wash_store(oscillator, 0x14, timer);
    bool restarted = false;
    if (timer <= 0.f)
    {
        wash_store(oscillator, 0x14, timer + period);
        std::memcpy(oscillator.data() + 0xC, oscillator.data() + 0x10, 4);
        restarted = true;
        if (callback)
        {
            callback(oscillator);
        }
    }
    if (read_f32(oscillator, 0xC) > 0.f)
    {
        const float angle = float(static_cast<long double>(read_f32(oscillator, 0xC))
            / read_f32(oscillator, 0x10) * std::bit_cast<float>(0x40490FDBu));
        const float wave = float(retail_sin(angle));
        retail_cos(angle);
        const float base = read_f32(oscillator, 4);
        wash_store(oscillator, 0, float(static_cast<long double>(wave)
            * read_f32(oscillator, 8) * base + base));
    }
    else
    {
        std::memcpy(oscillator.data(), oscillator.data() + 4, 4);
    }
    timer = read_f32(oscillator, 0xC) - elapsed;
    wash_store(oscillator, 0xC, timer);
    if (0.f > timer)
    {
        wash_store(oscillator, 0xC, 0.f);
    }
    return restarted;
}

// Xbox 0x0001E270
float runtime_body_health_fraction(std::span<const u8> body)
{
    if (read_u16(body, 0x96) == 0)
    {
        return 1.f;
    }
    return float(read_u16(body, 0x94)) / float(read_u16(body, 0x96));
}

// Xbox 0x0001E770
bool runtime_body_actor_requires_wash(std::span<const u8> actor, const u8* bound)
{
    if (read_u32(actor, 0x98C) != 0)
    {
        return false;
    }
    if (!(read_f32(actor, 0x978) > 0.f) && actor[0x969] == 0)
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
void update_runtime_body_wash_health(std::span<const u8> body, std::span<u8> wash)
{
    if (read_u16(body, 0x96) != 0)
    {
        const float fraction = float(read_u16(body, 0x94)) / float(read_u16(body, 0x96));
        if (0.25f > fraction)
        {
            wash_store(wash, 0x40, 4.f);
            return;
        }
        if (0.5f > fraction)
        {
            wash_store(wash, 0x40, 3.f);
            return;
        }
        if (0.75f > fraction)
        {
            wash_store(wash, 0x40, 2.f);
            return;
        }
    }
    wash_store(wash, 0x40, 1.f);
}

// Xbox 0x0001E380
void tick_runtime_body_wash_oscillator(std::span<u8> wash, const RuntimeActorClock& clock)
{
    if (read_u16(wash, 0) == 0)
    {
        wash_store(wash, 0x2C, 2.625f); // DAT_0032B2DC.
        wash_store(wash, 0x30, 0.f);
    }
    tick_runtime_body_oscillator(wash.subspan(0x28), runtime_frame_step(clock), {});
}

// Xbox 0x0001E6C0
void tick_runtime_body_wash_flashes(std::span<u8> wash, const RuntimeActorClock& clock)
{
    if (read_u16(wash, 0) == 0)
    {
        const float timer = read_f32(wash, 0x18) - runtime_frame_step(clock);
        wash_store(wash, 0x18, timer);
        if (timer <= 0.f)
        {
            wash_store(wash, 0x18, timer + 0.1f);
            wash[0x14] ^= 1;
        }
    }
    if (wash[0x16] != 0)
    {
        const float timer = read_f32(wash, 0x1C) - runtime_frame_step(clock);
        wash_store(wash, 0x1C, timer);
        if (timer <= 0.f)
        {
            wash_store(wash, 0x1C, timer + 0.75f);
            wash[0x15] ^= 1;
        }
    }
    else
    {
        wash[0x15] = 0;
    }
}

// Xbox 0x0001DD00
void tick_runtime_body_wash(std::span<u8> body, std::span<const u8> actor,
    const u8* bound, const RuntimeActorClock& clock)
{
    if (body[0xA6] == 0)
    {
        return;
    }
    auto wash = body.subspan(0x94);
    switch (wash[0x10])
    {
    case 0:
    {
        bool required = false;
        if (wash[0x13] == 0)
        {
            if (read_f32(wash, 0x20) > 0.f)
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
            wash_store(wash, 0x28, 1.75f);
            wash_store(wash, 0x2C, 1.75f);
            wash_store(wash, 0x30, 0.5f);
            wash_store(wash, 0x38, 0.2f);
            wash_store(wash, 0x40, 3.f);
            wash_store(wash, 0x34, 0.f);
            wash_store(wash, 0x3C, 0.f);
            wash[0x10] = 1;
            wash_store(wash, 0x24, 0.4f);
            wash[0x11] = 0;
            wash[0x14] = 1;
            update_runtime_body_wash_health(body, wash);
        }
        break;
    }
    case 1:
    {
        tick_runtime_body_wash_oscillator(wash, clock);
        tick_runtime_body_wash_flashes(wash, clock);
        update_runtime_body_wash_health(body, wash);
        float opacity = 128.f - (read_f32(wash, 0x24) * 2.5f) * 128.f;
        if (0.f > opacity)
        {
            opacity = 0.f;
        }
        else if (opacity > 128.f)
        {
            opacity = 128.f;
        }
        wash[0x11] = u8(truncate_runtime_float(opacity));
        float timer = read_f32(wash, 0x24);
        if (tick_runtime_body_frame_timer(timer, clock))
        {
            wash[0x10] = 2;
        }
        wash_store(wash, 0x24, timer);
        break;
    }
    case 2:
    {
        tick_runtime_body_wash_oscillator(wash, clock);
        const float fraction = runtime_body_health_fraction(body);
        wash_tick_timer(wash, 0x20, clock);
        tick_runtime_body_wash_flashes(wash, clock);
        update_runtime_body_wash_health(body, wash);
        wash[0x11] = 0x80;
        bool recovered = false;
        const float recovery_timer = read_f32(wash, 0x20);
        if (recovery_timer == 0.f && fraction > std::bit_cast<float>(0x3EAA7EFAu))
        {
            recovered = true;
        }
        else if (wash[0x13] != 0)
        {
            recovered = true;
        }
        if (!runtime_body_actor_requires_wash(actor, bound) && recovered)
        {
            wash[0x10] = 3;
            wash_store(wash, 0x24, 0.5f);
        }
        break;
    }
    case 3:
    {
        tick_runtime_body_wash_oscillator(wash, clock);
        update_runtime_body_wash_health(body, wash);
        float opacity = (read_f32(wash, 0x24) * 2.f) * 128.f;
        if (opacity > 128.f)
        {
            opacity = 128.f;
        }
        else if (0.f > opacity)
        {
            opacity = 0.f;
        }
        wash[0x11] = u8(truncate_runtime_float(opacity));
        float timer = read_f32(wash, 0x24);
        if (tick_runtime_body_frame_timer(timer, clock))
        {
            wash[0x10] = 0;
            wash[0x11] = 0;
        }
        wash_store(wash, 0x24, timer);
        break;
    }
    default:
        break;
    }
}
}
