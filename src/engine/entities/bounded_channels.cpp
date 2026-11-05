#include "bounded_channels.h"
#include "gbtg/anim.h"
#include "../graphics/extended_float.h"
#include <bit>
#include <cstring>

namespace gbtg
{
// Xbox 0x00101B00
void set_runtime_bounded_channel_limits(BoundedChannel& channel, float first,
    float second, const i32& pal, const i32& override_pal)
{
    float lower, upper;
    if (first > second)
    {
        lower = second;
        upper = first;
    }
    else if (second > first)
    {
        lower = first;
        upper = second;
    }
    else
    {
        channel.mode = u8(channel.mode & 0xFC);
        lower = second;
        upper = first;
    }
    channel.lower = lower;
    channel.upper = upper;
    if (channel.value > upper)
    {
        channel.value = upper;
    }
    else if (lower > channel.value)
    {
        channel.value = lower;
    }
    const i32 frequency = pal != 0 && override_pal == 0 ? 50 : 60;
    const float maximum = (float(frequency) * .5f) * (upper - lower);
    if (channel.rate > maximum)
    {
        channel.rate = maximum;
    }
}

// Xbox 0x00152BE0
long double runtime_float_remainder(float value, float divisor)
{
    const float divisor_magnitude = divisor < 0.f ? 0.f - divisor : divisor; // DAT_002038F0; COMISS; JC
    const float value_magnitude = value < 0.f ? 0.f - value : value;
    if (divisor_magnitude > value_magnitude)
    {
        return value;
    }
    const i32 quotient = i32(value / divisor); // DIVSS; CVTTSS2SI
    return static_cast<long double>(value) - static_cast<long double>(quotient) * divisor; // FILD; FMUL; FSUBR
}

// Xbox 0x00101B90
float approach_runtime_scalar(float current, float target, float amount)
{
    const float difference = float(ExtendedFloat{target} - ExtendedFloat{current});
    const float distance = float(engine_absolute(difference));
    if (amount == 0.0f)
    {
        return current;
    }
    if (amount > distance)
    {
        return target;
    }
    if (0.0f > difference)
    {
        return current - amount;
    }
    return current + amount;
}

// Xbox 0x00101C00
void approach_runtime_scalar_rate(float& current, float& rate, float target,
    float step)
{
    const float difference = float(ExtendedFloat{target} - ExtendedFloat{current});
    const float distance = float(engine_absolute(difference));
    const float amount = rate * step;
    if (rate != 0.0f)
    {
        if (amount > distance)
        {
            rate = 0.0f;
            current = target;
            return;
        }
        if (0.0f > difference)
        {
            current -= amount;
            return;
        }
        current += amount;
    }
}

// Xbox 0x00101430
bool runtime_wrapped_angles_equal(float first, float second)
{
    constexpr float turn = std::bit_cast<float>(0x40C90FDBu);
    constexpr float negative_pi = std::bit_cast<float>(0xC0490FDBu);
    constexpr float pi = std::bit_cast<float>(0x40490FDBu);
    float values[] = {first, second};
    for (float& value : values)
    {
        const long double remainder = animation_remainder(value, turn);
        value = float(remainder);
        if (negative_pi > remainder)
        {
            value += turn;
        }
        if (value >= pi)
        {
            value -= turn;
        }
    }
    return values[0] == values[1];
}

// Xbox 0x001063D0
void set_runtime_bounded_channel_limits(std::array<BoundedChannel, 3>& channels, const Vec3& first,
    const Vec3& second, const i32& pal, const i32& override_pal)
{
    const float* a[] = {&first.x, &first.y, &first.z};
    const float* b[] = {&second.x, &second.y, &second.z};
    for (size_t index = 0; index < 3; ++index)
    {
        BoundedChannel& row = channels[index];
        float lower, upper;
        if (*a[index] > *b[index])
        {
            lower = *b[index];
            upper = *a[index];
        }
        else if (*b[index] > *a[index])
        {
            lower = *a[index];
            upper = *b[index];
        }
        else
        {
            row.mode = u8(row.mode & 0xFC);
            lower = *b[index];
            upper = *a[index];
        }
        row.lower = lower;
        row.upper = upper;
        const float value = row.value;
        if (value > upper)
        {
            row.value = upper;
        }
        else if (lower > value)
        {
            row.value = lower;
        }
        const i32 frequency = pal != 0 && override_pal == 0 ? 50 : 60;
        const float maximum = (float(frequency) * .5f) *
            (row.upper - row.lower);
        if (row.rate > maximum)
        {
            row.rate = maximum;
        }
    }
}

// Xbox 0x00106590
void set_runtime_bounded_channel_rates(std::array<BoundedChannel, 3>& channels, const Vec3& rates,
    const i32& pal, const i32& override_pal)
{
    const float* values[] = {&rates.x, &rates.y, &rates.z};
    for (size_t index = 0; index < 3; ++index)
    {
        BoundedChannel& row = channels[index];
        const float value = *values[index];
        const float width = row.upper - row.lower;
        row.rate = value;
        const i32 frequency = pal != 0 && override_pal == 0 ? 50 : 60;
        const float maximum = (float(frequency) * .5f) * width;
        if (value > maximum)
        {
            row.rate = maximum;
        }
    }
}

// Xbox 0x00101980
void decrease_runtime_bounded_channel(BoundedChannel& channel, float step)
{
    const float value = channel.value - channel.rate * step;
    channel.value = value;
    if (channel.lower > value)
    {
        const u8 flags = channel.mode;
        if ((flags & 4) != 0)
        {
            channel.mode = u8((flags & 0xFD) | 1);
            channel.value = (channel.lower - value) + channel.lower;
        }
        else if ((flags & 8) != 0)
        {
            if (channel.lower > value)
            {
                const float width =
                    channel.upper - channel.lower;
                do
                {
                    channel.value = width + channel.value;
                }
                while (channel.lower > channel.value);
            }
        }
        else
        {
            channel.mode = u8(flags & 2);
            channel.value = channel.lower;
            return;
        }
        if (channel.value > channel.upper)
        {
            channel.value = channel.upper;
            return;
        }
        if (channel.lower > channel.value)
        {
            channel.value = channel.lower;
        }
    }
}

// Xbox 0x00101A40
void increase_runtime_bounded_channel(BoundedChannel& channel, float step)
{
    const float value = channel.rate * step + channel.value;
    channel.value = value;
    if (value > channel.upper)
    {
        const u8 flags = channel.mode;
        if ((flags & 4) != 0)
        {
            channel.mode = u8((flags & 0xFE) | 2);
            channel.value = channel.upper - (value - channel.upper);
        }
        else if ((flags & 8) != 0)
        {
            if (value > channel.upper)
            {
                const float width =
                    channel.upper - channel.lower;
                do
                {
                    channel.value = channel.value - width;
                }
                while (channel.value > channel.upper);
            }
        }
        else
        {
            channel.mode = u8(flags & 1);
            channel.value = channel.upper;
            return;
        }
        if (channel.value > channel.upper)
        {
            channel.value = channel.upper;
            return;
        }
        if (channel.lower > channel.value)
        {
            channel.value = channel.lower;
        }
    }
}

// Xbox 0x001062E0
void tick_runtime_bounded_channels(std::array<BoundedChannel, 3>& channels, float step, float* output)
{
    for (size_t index = 0; index < 3; ++index)
    {
        BoundedChannel& channel = channels[index];
        const u8 flags = channel.mode;
        if ((flags & 2) != 0)
        {
            decrease_runtime_bounded_channel(channel, step);
        }
        else if ((flags & 1) != 0)
        {
            increase_runtime_bounded_channel(channel, step);
        }
        if (output != nullptr)
        {
            const float value = channel.value;
            std::memcpy(output + index, &value, 4);
        }
    }
}
}
