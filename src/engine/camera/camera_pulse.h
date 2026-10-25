#pragma once
#include "gbtg/types.h"
#include "../entities/actor_clock.h"

#include <array>

namespace gbtg
{
// Full-view tint block at DAT_00504450: over `total` seconds the colour swings from `rise` (first half) or `fall`
// (second half) toward `peak` and back, and 0x000615E0 draws it while `shown` is set.
struct RuntimeScreenTint
{
    u32 rise; // DAT_00504450.
    u32 peak; // DAT_00504454.
    u32 fall; // DAT_00504458.
    std::array<float, 4> color; // DAT_0050445C..DAT_00504468, pixel shader constant 0 of the flash draw.
    float total; // DAT_0050446C.
    float remaining; // DAT_00504470; below zero shows `rise` for one frame.
    u32 active; // DAT_00504474.
    u32 shown; // DAT_00504478.
};

// Timed back-buffer blur block at DAT_0050447C: the radius and alpha run from their first to their second value
// over `total` seconds, and 0x000615E0 draws the captured view while `capture` is set.
struct RuntimeCameraPulse
{
    float total; // DAT_0050447C.
    float remaining; // DAT_00504480, ticked by 0x0012B2A0.
    float radius_from; // DAT_00504484.
    float radius_to; // DAT_00504488.
    float radius; // DAT_0050448C.
    float alpha_from; // DAT_00504490.
    float alpha_to; // DAT_00504494.
    float alpha; // DAT_00504498.
    u32 hold; // DAT_0050449C; nonzero keeps the end values once the time is up.
    u32 active; // DAT_005044A0.
    u32 capture; // DAT_005044A4.
};

// The retained border lines (DAT_005044A8 / DAT_005044AC / DAT_005044B0).
struct RuntimeScreenLines
{
    u32 color; // DAT_005044A8.
    float width; // DAT_005044AC.
    u32& retained; // DAT_005044B0.
};

void start_runtime_camera_pulse(float duration, float first, float second, float third, float fourth,
    u32 mode, const u32& runtime_flags, RuntimeCameraPulse& pulse); // Xbox 0x00061960, XMM0 duration
void clear_runtime_screen_tint(RuntimeScreenTint& tint); // Xbox 0x00060BC0
void clear_runtime_camera_pulse(RuntimeCameraPulse& pulse); // Xbox 0x00060C20
void reset_runtime_screen_effects(RuntimeScreenTint& tint, RuntimeCameraPulse& pulse,
    RuntimeScreenLines& lines); // Xbox 0x00060C80
void tick_runtime_screen_effects(RuntimeScreenTint& tint, RuntimeCameraPulse& pulse, const RuntimeActorClock& clock,
    const i32& transition_phase); // Xbox 0x00060D50
}
