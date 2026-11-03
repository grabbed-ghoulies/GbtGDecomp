#include "effect_channels.h"
#include "../entities/entity.h"
#include "extended_float.h"
#include "gbtg/anim.h"
#include "../entities/random_placement.h"
#include "../entities/bounded_channels.h"
#include <bit>
#include <cstring>

namespace gbtg
{
// Xbox 0x000F1D20
void configure_actor_effect_bounded_channels(std::span<u8> particle, const Vec3& first,
    const Vec3& second, const Vec3& position, const Vec3& rates, i32 random_position,
    const i32& pal, const i32& override_pal, u32& random)
{
    if (runtime_wrapped_angles_equal(first.x, second.x)
        && runtime_wrapped_angles_equal(first.y, second.y)
        && runtime_wrapped_angles_equal(first.z, second.z))
    {
        particle[0x58] &= 0xF7;
        return;
    }
    auto channels = particle.subspan(0x10C, 0x3C);
    const float zero = 0.f;
    for (size_t index = 0; index < 3; ++index)
    {
        auto row = channels.subspan(index * 0x14, 0x14);
        for (size_t offset : {0u, 4u, 8u, 12u})
        {
            std::memcpy(row.data() + offset, &zero, 4);
        }
        row[0x10] = 0;
    }
    set_runtime_bounded_channel_limits(channels, first, second, pal, override_pal);
    set_runtime_bounded_channel_rates(channels, rates, pal, override_pal);
    Vec3 initial;
    if (random_position == 0)
    {
        initial = position;
    }
    else
    {
        initial.x = sample_runtime_angle_range(second.x, first.x, random);
        initial.y = sample_runtime_angle_range(second.y, first.y, random);
        initial.z = sample_runtime_angle_range(second.z, first.z, random);
    }
    const float* values[] = {&initial.x, &initial.y, &initial.z};
    for (size_t index = 0; index < 3; ++index)
    {
        auto row = channels.subspan(index * 0x14, 0x14);
        float value = *values[index];
        std::memcpy(row.data() + 0xC, &value, 4);
        if (value > read_f32(row, 4))
        {
            value = read_f32(row, 4);
            std::memcpy(row.data() + 0xC, &value, 4);
        }
        else if (read_f32(row, 0) > value)
        {
            value = read_f32(row, 0);
            std::memcpy(row.data() + 0xC, &value, 4);
        }
    }
    for (size_t index = 0; index < 3; ++index)
    {
        auto row = channels.subspan(index * 0x14, 0x14);
        if (read_f32(row, 0) != read_f32(row, 4))
        {
            row[0x10] = 5;
        }
    }
    particle[0x58] |= 8;
}

// Xbox 0x000F2430
bool actor_effect_channels_are_active(std::span<const u8> channels)
{
    const auto rates = get(channels, effect_channel_field::rates);
    if (rates[0] == 0.f && rates[1] == 0.f && rates[2] == 0.f && rates[3] == 0.f && rates[4] == 0.f)
    {
        return false;
    }
    return true;
}

// Xbox 0x000F22F0
void target_actor_effect_color(std::span<u8> channels, u8 red, u8 green, u8 blue, float duration)
{
    constexpr float epsilon = std::bit_cast<float>(0x3456BF95u);
    set(channels, effect_channel_field::target_green, float(green));
    set(channels, effect_channel_field::target_blue, float(blue));
    set(channels, effect_channel_field::target_red, float(red));
    float* const values = field_data(channels, effect_channel_field::values);
    float* const rates = field_data(channels, effect_channel_field::rates);
    for (size_t index = 0; index < 3; ++index)
    {
        const float target = get(channels, effect_channel_field::targets)[index];
        if (duration > epsilon)
        {
            const float difference = float(ExtendedFloat{target} - ExtendedFloat{values[index]});
            rates[index] = float(ExtendedFloat{engine_absolute(difference)} / ExtendedFloat{duration});
        }
        else
        {
            values[index] = target;
            rates[index] = 0.f;
        }
    }
}

// Xbox 0x000F23E0
void target_actor_effect_alpha(std::span<u8> channels, u8 alpha, float duration)
{
    constexpr float epsilon = std::bit_cast<float>(0x3456BF95u);
    const float target = float(alpha);
    set(channels, effect_channel_field::target_alpha, target);
    if (duration > epsilon)
    {
        const float difference = float(ExtendedFloat{target}
            - ExtendedFloat{get(channels, effect_channel_field::alpha)});
        set(channels, effect_channel_field::alpha_rate,
            float(ExtendedFloat{engine_absolute(difference)} / ExtendedFloat{duration}));
        return;
    }
    set(channels, effect_channel_field::alpha, target);
    set(channels, effect_channel_field::alpha_rate, 0.f);
}

// Xbox 0x000F1AF0
void configure_actor_effect_oscillation(std::span<u8> particle, const Vec3& rate,
    const Vec3& phase, const Vec3& lower, const Vec3& upper, i32 random_phase, u32& random)
{
    if (lower.x == upper.x && lower.y == upper.y && lower.z == upper.z)
    {
        particle[0x58] &= 0xFB;
        return;
    }
    constexpr float degrees = std::bit_cast<float>(0x3C8EFA35u);
    constexpr float turn = std::bit_cast<float>(0x40C90FDBu);
    const Vec3 rates = rate;
    const float zero = 0.f;
    for (size_t offset : {0xC0u,0xBCu,0xB8u,0xCCu,0xC8u,0xC4u,0xD8u,0xD4u,0xD0u,0xE4u,0xE0u,0xDCu})
    {
        std::memcpy(particle.data() + offset, &zero, 4);
    }
    const Vec3 angular_rate{rates.x * degrees, rates.y * degrees, rates.z * degrees};
    for (size_t offset : {0xF0u,0xECu,0xE8u})
    {
        std::memcpy(particle.data() + offset, &zero, 4);
    }
    Vec3 angular_phase;
    if (random_phase == 0)
    {
        angular_phase = {phase.x * degrees, phase.y * degrees, phase.z * degrees};
    }
    else
    {
        angular_phase.x = sample_runtime_angle_range(turn, 0.f, random);
        angular_phase.y = sample_runtime_angle_range(turn, 0.f, random);
        angular_phase.z = sample_runtime_angle_range(turn, 0.f, random);
    }
    std::memcpy(particle.data() + 0xC4, &angular_phase, 12);
    std::memcpy(particle.data() + 0xB8, &angular_rate, 12);
    const float* first[] = {&lower.x, &lower.y, &lower.z};
    const float* second[] = {&upper.x, &upper.y, &upper.z};
    for (size_t index = 0; index < 3; ++index)
    {
        const float amplitude = (*second[index] - *first[index]) * .5f;
        std::memcpy(particle.data() + 0xDC + index * 4, &amplitude, 4);
        const float center = amplitude + *first[index];
        std::memcpy(particle.data() + 0xD0 + index * 4, &center, 4);
    }
    particle[0x58] |= 4;
}

// Xbox 0x00106A80
void tick_actor_effect_oscillation(std::span<u8> channels, float step)
{
    constexpr float turn = std::bit_cast<float>(0x40C90FDBu);
    for (size_t index = 0; index < 3; ++index)
    {
        const float phase = float(ExtendedFloat{step} * ExtendedFloat{read_f32(channels, index * 4)}
            + ExtendedFloat{read_f32(channels, 0xC + index * 4)});
        const long double remainder = animation_remainder(phase, turn);
        float wrapped = float(remainder);
        if (0.f > remainder)
        {
            wrapped += turn;
        }
        std::memcpy(channels.data() + 0xC + index * 4, &wrapped, 4);
    }
    for (size_t index = 0; index < 3; ++index)
    {
        const ExtendedFloat sine{double(retail_sin(read_f32(channels, 0xC + index * 4)))};
        const float value = float(sine * ExtendedFloat{read_f32(channels, 0x24 + index * 4)}
            + ExtendedFloat{read_f32(channels, 0x18 + index * 4)});
        std::memcpy(channels.data() + 0x30 + index * 4, &value, 4);
    }
}

// Xbox 0x000F1F60
void approach_actor_effect_channels(std::span<u8> channels, const RuntimeActorClock& clock)
{
    const float dt = runtime_frame_step(clock);
    float* const values = field_data(channels, effect_channel_field::values);
    const float* const targets = field_data(channels, effect_channel_field::targets);
    float* const rates = field_data(channels, effect_channel_field::rates);
    for (size_t index = 0; index < 5; ++index)
    {
        if (rates[index] != 0.f)
        {
            const float target = targets[index];
            const float difference = target - values[index];
            const float distance = engine_absolute(difference);
            const float step = rates[index] * dt;
            if (rates[index] != 0.f)
            {
                if (step > distance)
                {
                    rates[index] = 0.f;
                    values[index] = target;
                }
                else
                {
                    const float current = values[index];
                    values[index] = 0.f > difference ? current - step : current + step;
                }
            }
            float value = values[index];
            if (0.f > value)
            {
                value = 0.f;
            }
            else if (value > 255.f)
            {
                value = 255.f;
            }
            values[index] = value;
        }
    }
}
}
