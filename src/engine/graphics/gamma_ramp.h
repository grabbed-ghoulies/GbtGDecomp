#pragma once
#include "gbtg/types.h"

#include <array>

namespace gbtg
{
// The display's gamma settings (DAT_005044B8 .. DAT_005044CC) and the D3DGAMMARAMP built from them
// (DAT_005044D0, DAT_005045D0, DAT_005046D0: red, green and blue, 256 bytes each).
struct RuntimeGammaRamp
{
    float red = 0.f; // DAT_005044B8, each channel's gamma
    float green = 0.f; // DAT_005044BC
    float blue = 0.f; // DAT_005044C0
    float scale = 0.f; // DAT_005044C4, multiplies the three
    float brightness = 0.f; // DAT_005044C8, 1 adds nothing
    float contrast = 0.f; // DAT_005044CC, 1 leaves the slope
    std::array<std::array<u8, 256>, 3> ramp{};
};
extern RuntimeGammaRamp g_gamma_ramp;

// Entry `index` of a channel: (index / 255) ^ (1 / gamma), scaled by contrast * 255 and raised by (brightness - 1) *
// 255, kept in 0..255 and truncated.
u8 gamma_ramp_entry(i32 index, float gamma, float brightness, float contrast); // Xbox 0x00061BC0
// Builds the three channels and hands them to D3DDevice_SetGammaRamp.
void apply_gamma_ramp(RuntimeGammaRamp& gamma); // Xbox 0x00061C40
// Boot 0x00033030: every gamma 1, then 0x00061C40; brightness and contrast that are not 1 become 1 and the ramp is
// built again.
void boot_gamma_ramp(RuntimeGammaRamp& gamma);
}
