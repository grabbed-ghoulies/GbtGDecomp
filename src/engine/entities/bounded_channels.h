#pragma once
#include "gbtg/entity.h"
#include "gbtg/retail_field.h"
#include <array>

namespace gbtg
{
// A bounded channel (0x14 bytes): a value moved between two limits at a rate (0x00101980, 0x00101A40).
struct BoundedChannel
{
    float lower = 0.f; // +0
    float upper = 0.f; // +4
    float rate = 0.f; // +8, per second, at most half a frame rate's worth of the range
    float value = 0.f; // +0xC
    u8 mode = 0; // +0x10, 1 counts up, 2 counts down; 4 bounces off a limit, 8 wraps around, neither stops at it
    std::array<u8, 3> unk_11{}; // +0x11, never read or written
};
static_assert(sizeof(BoundedChannel) == 0x14);

float approach_runtime_scalar(float current, float target, float amount); // Xbox 0x00101B90
// `value` less the whole multiples of `divisor` (the quotient truncated, x87 product), or `value` when the divisor's
// magnitude is the larger.
long double runtime_float_remainder(float value, float divisor); // Xbox 0x00152BE0
void set_runtime_bounded_channel_limits(BoundedChannel& channel, float first,
    float second, const i32& pal, const i32& override_pal); // Xbox 0x00101B00
void approach_runtime_scalar_rate(float& current, float& rate, float target,
    float step); // Xbox 0x00101C00
bool runtime_wrapped_angles_equal(float first, float second); // Xbox 0x00101430
void set_runtime_bounded_channel_limits(std::array<BoundedChannel, 3>& channels, const Vec3& first,
    const Vec3& second, const i32& pal, const i32& override_pal); // Xbox 0x001063D0
void set_runtime_bounded_channel_rates(std::array<BoundedChannel, 3>& channels, const Vec3& rates,
    const i32& pal, const i32& override_pal); // Xbox 0x00106590
void decrease_runtime_bounded_channel(BoundedChannel& channel, float step); // Xbox 0x00101980
void increase_runtime_bounded_channel(BoundedChannel& channel, float step); // Xbox 0x00101A40
void tick_runtime_bounded_channels(std::array<BoundedChannel, 3>& channels, float step,
    float* output); // Xbox 0x001062E0
}
