#pragma once
#include "gbtg/entity.h"
#include <span>

namespace gbtg
{
float approach_runtime_scalar(float current, float target, float amount); // Xbox 0x00101B90
void set_runtime_bounded_channel_limits(std::span<u8> channel, float first,
    float second, const i32& pal, const i32& override_pal); // Xbox 0x00101B00
void approach_runtime_scalar_rate(float& current, float& rate, float target,
    float step); // Xbox 0x00101C00
bool runtime_wrapped_angles_equal(float first, float second); // Xbox 0x00101430
void set_runtime_bounded_channel_limits(std::span<u8> channels, const Vec3& first,
    const Vec3& second, const i32& pal, const i32& override_pal); // Xbox 0x001063D0
void set_runtime_bounded_channel_rates(std::span<u8> channels, const Vec3& rates,
    const i32& pal, const i32& override_pal); // Xbox 0x00106590
void decrease_runtime_bounded_channel(std::span<u8> channel, float step); // Xbox 0x00101980
void increase_runtime_bounded_channel(std::span<u8> channel, float step); // Xbox 0x00101A40
void tick_runtime_bounded_channels(std::span<u8> channels, float step,
    float* output); // Xbox 0x001062E0
}
