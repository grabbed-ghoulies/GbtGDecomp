#pragma once
#include "gbtg/types.h"

namespace gbtg
{
// Timed camera pulse block at DAT_0050447C.
struct RuntimeCameraPulse
{
    float remaining; // DAT_0050447C.
    float duration; // DAT_00504480.
    float first; // DAT_00504484.
    float second; // DAT_00504488.
    float third; // DAT_00504490.
    float fourth; // DAT_00504494.
    u32 mode; // DAT_0050449C.
    u32 active; // DAT_005044A0.
};
void start_runtime_camera_pulse(float duration, float first, float second, float third, float fourth,
    u32 mode, const u32& runtime_flags, RuntimeCameraPulse& pulse); // Xbox 0x00061960, XMM0 duration
}
