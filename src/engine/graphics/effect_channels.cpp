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
void configure_actor_effect_bounded_channels(EffectParticle& particle, const Vec3& first,
    const Vec3& second, const Vec3& position, const Vec3& rates, i32 random_position,
    const i32& pal, const i32& override_pal, u32& random)
{
    if (runtime_wrapped_angles_equal(first.x, second.x)
        && runtime_wrapped_angles_equal(first.y, second.y)
        && runtime_wrapped_angles_equal(first.z, second.z))
    {
        particle.channel_flags &= 0xFFF7;
        return;
    }
    for (BoundedChannel& channel : particle.bounded_channels)
    {
        channel.lower = 0.f;
        channel.upper = 0.f;
        channel.rate = 0.f;
        channel.value = 0.f;
        channel.mode = 0;
    }
    auto& channels = particle.bounded_channels;
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
        BoundedChannel& channel = particle.bounded_channels[index];
        float value = *values[index];
        channel.value = value;
        if (value > channel.upper)
        {
            value = channel.upper;
            channel.value = value;
        }
        else if (channel.lower > value)
        {
            value = channel.lower;
            channel.value = value;
        }
    }
    for (BoundedChannel& channel : particle.bounded_channels)
    {
        if (channel.lower != channel.upper)
        {
            channel.mode = 5;
        }
    }
    particle.channel_flags |= 8;
}

// Xbox 0x000F2430
bool actor_effect_channels_are_active(const EffectColourChannels& channels)
{
    const auto rates = channels.rates;
    if (rates[0] == 0.f && rates[1] == 0.f && rates[2] == 0.f && rates[3] == 0.f && rates[4] == 0.f)
    {
        return false;
    }
    return true;
}

// Xbox 0x000F22F0
void target_actor_effect_color(EffectColourChannels& channels, u8 red, u8 green, u8 blue, float duration)
{
    constexpr float epsilon = std::bit_cast<float>(0x3456BF95u);
    channels.target_green() = float(green);
    channels.target_blue() = float(blue);
    channels.target_red() = float(red);
    float* const values = channels.values.data();
    float* const rates = channels.rates.data();
    for (size_t index = 0; index < 3; ++index)
    {
        const float target = channels.targets[index];
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
void target_actor_effect_alpha(EffectColourChannels& channels, u8 alpha, float duration)
{
    constexpr float epsilon = std::bit_cast<float>(0x3456BF95u);
    const float target = float(alpha);
    channels.target_alpha() = target;
    if (duration > epsilon)
    {
        const float difference = float(ExtendedFloat{target}
            - ExtendedFloat{channels.alpha()});
        channels.alpha_rate() = float(ExtendedFloat{engine_absolute(difference)} / ExtendedFloat{duration});
        return;
    }
    channels.alpha() = target;
    channels.alpha_rate() = 0.f;
}

// Xbox 0x000F1AF0
void configure_actor_effect_oscillation(EffectParticle& particle, const Vec3& rate,
    const Vec3& phase, const Vec3& lower, const Vec3& upper, i32 random_phase, u32& random)
{
    if (lower.x == upper.x && lower.y == upper.y && lower.z == upper.z)
    {
        particle.channel_flags &= 0xFFFB;
        return;
    }
    constexpr float degrees = std::bit_cast<float>(0x3C8EFA35u);
    constexpr float turn = std::bit_cast<float>(0x40C90FDBu);
    EffectOscillation& oscillation = particle.oscillation;
    const Vec3 rates = rate;
    oscillation.rate = {};
    oscillation.phase = {};
    oscillation.center = {};
    oscillation.amplitude = {};
    const Vec3 angular_rate{rates.x * degrees, rates.y * degrees, rates.z * degrees};
    oscillation.value = {};
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
    oscillation.phase = angular_phase;
    oscillation.rate = angular_rate;
    const float* first[] = {&lower.x, &lower.y, &lower.z};
    const float* second[] = {&upper.x, &upper.y, &upper.z};
    float* const amplitudes[] = {&oscillation.amplitude.x, &oscillation.amplitude.y, &oscillation.amplitude.z};
    float* const centers[] = {&oscillation.center.x, &oscillation.center.y, &oscillation.center.z};
    for (size_t index = 0; index < 3; ++index)
    {
        const float amplitude = (*second[index] - *first[index]) * .5f;
        *amplitudes[index] = amplitude;
        *centers[index] = amplitude + *first[index];
    }
    particle.channel_flags |= 4;
}

// Xbox 0x00106A80
void tick_actor_effect_oscillation(EffectOscillation& oscillation, float step)
{
    constexpr float turn = std::bit_cast<float>(0x40C90FDBu);
    const float rates[] = {oscillation.rate.x, oscillation.rate.y, oscillation.rate.z};
    float* const phases[] = {&oscillation.phase.x, &oscillation.phase.y, &oscillation.phase.z};
    const float centers[] = {oscillation.center.x, oscillation.center.y, oscillation.center.z};
    const float amplitudes[] = {oscillation.amplitude.x, oscillation.amplitude.y, oscillation.amplitude.z};
    float* const values[] = {&oscillation.value.x, &oscillation.value.y, &oscillation.value.z};
    for (size_t index = 0; index < 3; ++index)
    {
        const float phase = float(ExtendedFloat{step} * ExtendedFloat{rates[index]} + ExtendedFloat{*phases[index]});
        const long double remainder = animation_remainder(phase, turn);
        float wrapped = float(remainder);
        if (0.f > remainder)
        {
            wrapped += turn;
        }
        *phases[index] = wrapped;
    }
    for (size_t index = 0; index < 3; ++index)
    {
        const ExtendedFloat sine{double(retail_sin(*phases[index]))};
        *values[index] = float(sine * ExtendedFloat{amplitudes[index]} + ExtendedFloat{centers[index]});
    }
}

// Xbox 0x000F1F60
void approach_actor_effect_channels(EffectColourChannels& channels, const RuntimeActorClock& clock)
{
    const float dt = runtime_frame_step(clock);
    float* const values = channels.values.data();
    const float* const targets = channels.targets.data();
    float* const rates = channels.rates.data();
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
