#include "camera_pulse.h"
#include "gbtg/anim.h"
#include "../entities/body_timers.h"
#include "../graphics/color_blend.h"

#include <bit>

namespace gbtg
{
namespace
{
constexpr u32 kBlack = 0xFF000000u; // DAT_00314F4C.
constexpr u32 kWhite = 0xFFFFFFFFu; // DAT_00314F48.
constexpr float kPi = std::bit_cast<float>(0x40490FDBu); // DAT_0032B148.
constexpr float kByte = std::bit_cast<float>(0x3B808081u); // DAT_0032B154, 1/255.

// 0x00060D50's inlined blend of one pulse pair: `from` to `to` by the time gone, held between the two.
float blend_pulse_value(float from, float to, float fraction)
{
    const float value = fraction * (to - from) + from;
    if (from < to)
    {
        if (value > to)
        {
            return to;
        }
        if (from > value)
        {
            return from;
        }
        return value;
    }
    if (to > value)
    {
        return to;
    }
    if (value > from)
    {
        return from;
    }
    return value;
}
}

// Xbox 0x00061960
void start_runtime_camera_pulse(float duration, float first, float second, float third, float fourth,
    u32 mode, const u32& runtime_flags, RuntimeCameraPulse& pulse)
{
    if ((runtime_flags & 0x8000) == 0)
    {
        pulse.total = duration;
        pulse.remaining = duration;
        pulse.radius_from = first;
        pulse.radius_to = second;
        pulse.alpha_from = third;
        pulse.alpha_to = fourth;
        pulse.active = 1;
        pulse.hold = mode;
    }
}

// Xbox 0x00060BC0
void clear_runtime_screen_tint(RuntimeScreenTint& tint)
{
    tint.rise = kBlack;
    tint.peak = kBlack;
    tint.fall = kBlack;
    tint.color = {};
    tint.total = 0.f;
    tint.remaining = 0.f;
    tint.active = 0;
    tint.shown = 0;
}

// Xbox 0x00060C20
void clear_runtime_camera_pulse(RuntimeCameraPulse& pulse)
{
    pulse.radius_from = 0.f;
    pulse.radius_to = 0.f;
    pulse.radius = 0.f;
    pulse.alpha_from = 0.f;
    pulse.alpha_to = 0.f;
    pulse.alpha = 0.f;
    pulse.active = 0;
    pulse.capture = 0;
    pulse.hold = 0;
    pulse.total = 0.f;
    pulse.remaining = 0.f;
}

// Xbox 0x00060C80
void reset_runtime_screen_effects(RuntimeScreenTint& tint, RuntimeCameraPulse& pulse, RuntimeScreenLines& lines)
{
    tint.rise = kBlack;
    tint.peak = kBlack;
    tint.fall = kBlack;
    tint.color = {};
    tint.total = 0.f;
    tint.remaining = 0.f;
    pulse.radius_from = 0.f;
    pulse.radius_to = 0.f;
    pulse.radius = 0.f;
    pulse.alpha_from = 0.f;
    pulse.alpha_to = 0.f;
    pulse.alpha = 0.f;
    pulse.total = 0.f;
    pulse.remaining = 0.f;
    tint.active = 0;
    tint.shown = 0;
    pulse.active = 0;
    pulse.capture = 0;
    pulse.hold = 0;
    lines.color = kWhite;
    lines.width = 1.f; // DAT_0032B0F8.
    lines.retained = 0;
}

// Xbox 0x00060D50
void tick_runtime_screen_effects(RuntimeScreenTint& tint, RuntimeCameraPulse& pulse, const RuntimeActorClock& clock,
    const i32& transition_phase)
{
    bool tinted = true;
    if (tint.active != 0)
    {
        u32 color = tint.rise;
        if (!(tint.remaining < 0.f))
        {
            const float step = runtime_frame_step(clock);
            if (tint.remaining > 0.f)
            {
                tint.remaining -= step;
                if (tint.remaining <= 0.f)
                {
                    tint.remaining = 0.f;
                    clear_runtime_screen_tint(tint); // 0x00060BC0
                    tinted = false;
                }
            }
            if (tinted)
            {
                float angle = kPi;
                if (tint.total != 0.f)
                {
                    angle = (tint.remaining - tint.total) / (0.f - tint.total) * kPi;
                    if (angle > kPi)
                    {
                        angle = kPi;
                    }
                    else if (0.f > angle)
                    {
                        angle = 0.f;
                    }
                }
                const float swing = float(retail_sin(angle)); // 0x00153320
                const u32 half = tint.total * 0.5f > tint.remaining ? tint.fall : tint.rise; // DAT_0032B0F4
                color = menu_color(tint.peak, half, swing); // 0x00101730
            }
        }
        else
        {
            tint.rise = kBlack;
            tint.peak = kBlack;
            tint.fall = kBlack;
            tint.total = 0.f;
            tint.remaining = 0.f;
            tint.active = 0;
        }
        if (tinted)
        {
            tint.color[0] = float(color & 0xFF) * kByte;
            tint.color[1] = float((color >> 8) & 0xFF) * kByte;
            tint.color[2] = float((color >> 16) & 0xFF) * kByte;
            tint.color[3] = float(color >> 24) * kByte;
            tint.shown = 1;
        }
    }
    if (pulse.active == 0)
    {
        return;
    }
    if (pulse.remaining < 0.f)
    {
        const float radius = pulse.radius_from;
        const float alpha = pulse.alpha_from;
        pulse.capture = 1;
        pulse.radius_from = 0.f;
        pulse.radius_to = 0.f;
        pulse.alpha_from = 0.f;
        pulse.alpha_to = 0.f;
        pulse.active = 0;
        pulse.hold = 0;
        pulse.total = 0.f;
        pulse.remaining = 0.f;
        pulse.radius = radius;
        pulse.alpha = alpha;
        return;
    }
    if (transition_phase != 3) // DAT_0054632C.
    {
        if (pulse.remaining != 0.f && !tick_runtime_body_frame_timer(pulse.remaining, clock)) // 0x0012B2A0
        {
            pulse.capture = 1;
        }
        else if (pulse.hold == 0)
        {
            clear_runtime_camera_pulse(pulse); // 0x00060C20
            if (pulse.capture == 0)
            {
                return;
            }
        }
        else
        {
            pulse.capture = 1;
        }
    }
    else
    {
        pulse.capture = 1;
    }
    if (pulse.total == 0.f)
    {
        pulse.radius = pulse.radius_to;
        pulse.alpha = pulse.alpha_to;
        return;
    }
    const float fraction = (pulse.remaining - pulse.total) / (0.f - pulse.total);
    pulse.radius = blend_pulse_value(pulse.radius_from, pulse.radius_to, fraction);
    pulse.alpha = blend_pulse_value(pulse.alpha_from, pulse.alpha_to, fraction);
}
}
