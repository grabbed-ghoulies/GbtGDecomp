#include "bounded_channels.h"
#include "gbtg/anim.h"
#include "../graphics/extended_float.h"
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
void store_channel_value(std::span<u8> channel, float value)
{
    std::memcpy(channel.data() + 0xC, &value, 4);
}
}

// Xbox 0x00101B00
void set_runtime_bounded_channel_limits(std::span<u8> channel, float first,
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
        channel[0x10] &= 0xFC;
        lower = second;
        upper = first;
    }
    std::memcpy(channel.data(), &lower, 4);
    std::memcpy(channel.data() + 4, &upper, 4);
    if (read_f32(channel, 0xC) > upper)
    {
        store_channel_value(channel, upper);
    }
    else if (lower > read_f32(channel, 0xC))
    {
        store_channel_value(channel, lower);
    }
    const i32 frequency = pal != 0 && override_pal == 0 ? 50 : 60;
    const float maximum = (float(frequency) * .5f) * (upper - lower);
    if (read_f32(channel, 8) > maximum)
    {
        std::memcpy(channel.data() + 8, &maximum, 4);
    }
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
void set_runtime_bounded_channel_limits(std::span<u8> channels, const Vec3& first,
    const Vec3& second, const i32& pal, const i32& override_pal)
{
    const float* a[] = {&first.x, &first.y, &first.z};
    const float* b[] = {&second.x, &second.y, &second.z};
    for (size_t index = 0; index < 3; ++index)
    {
        auto row = channels.subspan(index * 0x14, 0x14);
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
            row[0x10] &= 0xFC;
            lower = *b[index];
            upper = *a[index];
        }
        std::memcpy(row.data(), &lower, 4);
        std::memcpy(row.data() + 4, &upper, 4);
        const float value = read_f32(row, 0xC);
        if (value > upper)
        {
            store_channel_value(row, upper);
        }
        else if (lower > value)
        {
            store_channel_value(row, lower);
        }
        const i32 frequency = pal != 0 && override_pal == 0 ? 50 : 60;
        const float maximum = (float(frequency) * .5f) * (read_f32(row, 4) - read_f32(row, 0));
        if (read_f32(row, 8) > maximum)
        {
            std::memcpy(row.data() + 8, &maximum, 4);
        }
    }
}

// Xbox 0x00106590
void set_runtime_bounded_channel_rates(std::span<u8> channels, const Vec3& rates,
    const i32& pal, const i32& override_pal)
{
    const float* values[] = {&rates.x, &rates.y, &rates.z};
    for (size_t index = 0; index < 3; ++index)
    {
        auto row = channels.subspan(index * 0x14, 0x14);
        const float value = *values[index];
        const float width = read_f32(row, 4) - read_f32(row, 0);
        std::memcpy(row.data() + 8, &value, 4);
        const i32 frequency = pal != 0 && override_pal == 0 ? 50 : 60;
        const float maximum = (float(frequency) * .5f) * width;
        if (value > maximum)
        {
            std::memcpy(row.data() + 8, &maximum, 4);
        }
    }
}

// Xbox 0x00101980
void decrease_runtime_bounded_channel(std::span<u8> channel, float step)
{
    const float value = read_f32(channel, 0xC) - read_f32(channel, 8) * step;
    store_channel_value(channel, value);
    if (read_f32(channel, 0) > value)
    {
        const u8 flags = channel[0x10];
        if ((flags & 4) != 0)
        {
            channel[0x10] = (flags & 0xFD) | 1;
            store_channel_value(channel, (read_f32(channel, 0) - value) + read_f32(channel, 0));
        }
        else if ((flags & 8) != 0)
        {
            if (read_f32(channel, 0) > value)
            {
                const float width = read_f32(channel, 4) - read_f32(channel, 0);
                do
                {
                    store_channel_value(channel, width + read_f32(channel, 0xC));
                }
                while (read_f32(channel, 0) > read_f32(channel, 0xC));
            }
        }
        else
        {
            channel[0x10] = flags & 2;
            store_channel_value(channel, read_f32(channel, 0));
            return;
        }
        if (read_f32(channel, 0xC) > read_f32(channel, 4))
        {
            store_channel_value(channel, read_f32(channel, 4));
            return;
        }
        if (read_f32(channel, 0) > read_f32(channel, 0xC))
        {
            store_channel_value(channel, read_f32(channel, 0));
        }
    }
}

// Xbox 0x00101A40
void increase_runtime_bounded_channel(std::span<u8> channel, float step)
{
    const float value = read_f32(channel, 8) * step + read_f32(channel, 0xC);
    store_channel_value(channel, value);
    if (value > read_f32(channel, 4))
    {
        const u8 flags = channel[0x10];
        if ((flags & 4) != 0)
        {
            channel[0x10] = (flags & 0xFE) | 2;
            store_channel_value(channel, read_f32(channel, 4) - (value - read_f32(channel, 4)));
        }
        else if ((flags & 8) != 0)
        {
            if (value > read_f32(channel, 4))
            {
                const float width = read_f32(channel, 4) - read_f32(channel, 0);
                do
                {
                    store_channel_value(channel, read_f32(channel, 0xC) - width);
                }
                while (read_f32(channel, 0xC) > read_f32(channel, 4));
            }
        }
        else
        {
            channel[0x10] = flags & 1;
            store_channel_value(channel, read_f32(channel, 4));
            return;
        }
        if (read_f32(channel, 0xC) > read_f32(channel, 4))
        {
            store_channel_value(channel, read_f32(channel, 4));
            return;
        }
        if (read_f32(channel, 0) > read_f32(channel, 0xC))
        {
            store_channel_value(channel, read_f32(channel, 0));
        }
    }
}

// Xbox 0x001062E0
void tick_runtime_bounded_channels(std::span<u8> channels, float step, float* output)
{
    for (size_t index = 0; index < 3; ++index)
    {
        auto channel = channels.subspan(index * 0x14, 0x14);
        const u8 flags = channel[0x10];
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
            const u32 value = read_u32(channel, 0xC);
            std::memcpy(output + index, &value, 4);
        }
    }
}
}
