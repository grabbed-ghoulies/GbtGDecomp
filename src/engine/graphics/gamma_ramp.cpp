#include "gamma_ramp.h"
#include "../audio/listener_settings.h"
#include "gbtg/gfx.h"

#include <cmath>

namespace gbtg
{
RuntimeGammaRamp g_gamma_ramp;

namespace
{
constexpr double kByteStep = 1.0 / 255.0; // DAT_0032B1D8
constexpr float kByteScale = 255.f; // DAT_0032B158
constexpr double kRampLow = 0.0; // DAT_0032B1D0
constexpr double kRampHigh = 255.0; // DAT_0032B1C8

// D3DDevice_SetGammaRamp(0, ramp).
void set_device_gamma_ramp(const std::array<std::array<u8, 256>, 3>& ramp)
{
    gfx_set_gamma_ramp(ramp[0].data(), ramp[1].data(), ramp[2].data());
}
} // namespace

// Xbox 0x00061BC0
u8 gamma_ramp_entry(i32 index, float gamma, float brightness, float contrast)
{
    // 0x001529A0 is the C runtime's pow.
    const float level = float(double(index) * kByteStep);
    const long double curve = std::pow(double(level), double(1.f / gamma)); // float arguments, x87 result
    long double value = (static_cast<long double>(brightness) - 1.0L) * kByteScale
        + static_cast<long double>(contrast) * kByteScale * curve;
    if (value < kRampLow)
    {
        value = kRampLow;
    }
    else if (value > kRampHigh)
    {
        value = kRampHigh;
    }
    return u8(truncate_runtime_float(value)); // 0x001673D8
}

// Xbox 0x00061C40
void apply_gamma_ramp(RuntimeGammaRamp& gamma)
{
    for (i32 index = 0; index < 0x100; ++index)
    {
        gamma.ramp[0][size_t(index)] = gamma_ramp_entry(index, gamma.scale * gamma.red, gamma.brightness,
            gamma.contrast);
        gamma.ramp[1][size_t(index)] = gamma_ramp_entry(index, gamma.green * gamma.scale, gamma.brightness,
            gamma.contrast);
        gamma.ramp[2][size_t(index)] = gamma_ramp_entry(index, gamma.blue * gamma.scale, gamma.brightness,
            gamma.contrast);
    }
    set_device_gamma_ramp(gamma.ramp);
}

void boot_gamma_ramp(RuntimeGammaRamp& gamma)
{
    gamma.red = 1.f; // DAT_0032B0F8
    gamma.green = 1.f;
    gamma.blue = 1.f;
    gamma.scale = 1.f;
    apply_gamma_ramp(gamma);
    if (gamma.brightness != 1.f || gamma.contrast != 1.f)
    {
        gamma.brightness = 1.f;
        gamma.contrast = 1.f;
        apply_gamma_ramp(gamma);
    }
}
} // namespace gbtg
