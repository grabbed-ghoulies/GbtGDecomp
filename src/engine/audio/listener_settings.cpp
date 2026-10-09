#include "listener_settings.h"
#include <bit>
#include <cmath>
#include <cstring>

namespace gbtg
{
// Xbox 0x00058B90
void set_dialogue_audio_duck(AudioListenerSettings& state, i32 enabled,
    u32 scene_flags, u32 first_category, u32 second_category)
{
    float second;
    float first;
    if (enabled == 0)
    {
        second = 1.f;
        first = 1.f;
    }
    else
    {
        second = 0.5f;
        first = 0.f;
    }
    if ((scene_flags & 0x20) == 0)
    {
        first *= 127.f;
        if (first > 127.f)
        {
            first = 127.f;
        }
        else if (first < 0.f)
        {
            first = 0.f;
        }
        set_audio_category_volume(state, first_category, i32(first));
        second *= 127.f;
        if (second > 127.f)
        {
            second = 127.f;
        }
        else if (second < 0.f)
        {
            second = 0.f;
        }
        set_audio_category_volume(state, second_category, i32(second));
    }
}

// Xbox 0x001673D8
std::int64_t truncate_runtime_float(long double value)
{
    const float saved = float(value);
    const long double rounded = std::nearbyint(value);
    std::int64_t integer;
    // FISTP stores integer indefinite for NaN or values outside signed qword.
    if (!(rounded >= -9223372036854775808.0L && rounded < 9223372036854775808.0L))
    {
        integer = std::bit_cast<std::int64_t>(u64(0x8000000000000000ull));
    }
    else
    {
        integer = std::int64_t(rounded);
    }
    u64 bits = std::bit_cast<u64>(integer);
    const u32 low = u32(bits);
    const u32 high = u32(bits >> 32);
    if (low != 0 || (high & 0x7FFFFFFFu) != 0)
    {
        u32 remainder = std::bit_cast<u32>(float(value - static_cast<long double>(integer)));
        if ((std::bit_cast<u32>(saved) & 0x80000000u) != 0)
        {
            remainder ^= 0x80000000u;
            bits += u64(remainder) + 0x7FFFFFFFull > 0xFFFFFFFFull;
        }
        else
        {
            bits -= u64(remainder) + 0x7FFFFFFFull > 0xFFFFFFFFull;
        }
    }
    return std::bit_cast<std::int64_t>(bits);
}

// Xbox 0x0015B8F0
i32 audio_volume_attenuation(i32 volume)
{
    if (volume == 0)
    {
        return -10000;
    }
    // FLDLG2/FYL2X computes log10; double at Xbox 0x00203D50 is -1000.
    const long double logarithm = std::log10(127.0L / static_cast<long double>(volume));
    return std::bit_cast<i32>(u32(truncate_runtime_float(logarithm * -1000.0L)));
}

// Xbox 0x0015C1C0
void set_audio_category_volume(AudioListenerSettings& state, u32 category, i32 volume)
{
    const i32 attenuation = audio_volume_attenuation(volume);
    state.master_volume(category, attenuation);
}

// Xbox 0x0015C1E0
void set_audio_listener(AudioListenerSettings& state, const std::array<u32, 12>& parameters)
{
    state.parameters[2] = parameters[2];
    state.parameters[3] = parameters[3];
    state.parameters[4] = parameters[4];
    state.parameters[6] = parameters[6];
    state.parameters[1] = parameters[1];
    state.parameters[8] = parameters[8];
    state.parameters[9] = parameters[9];
    state.parameters[5] = parameters[5];
    state.parameters[0] = parameters[0];
    state.parameters[10] = parameters[10];
    state.parameters[7] = parameters[7];
    state.parameters[11] = parameters[11];
    state.listener(state.parameters, state.apply);
}

// Xbox 0x0003A1F0
void set_audio_reverb_preset(AudioListenerSettings& state, i32 preset,
    AnimationResourceRegistry& registry, ResourceErrorState& error)
{
    const char* name = "aid_misc_ghoulies_audio_reverbsettings";
    std::span<const u8> resource;
    if (read_u32({reinterpret_cast<const u8*>(name), 4}, 0) == 0x5F646961u)
    {
        resource = acquire_animation_resource(registry, name);
        if (resource.data() == nullptr)
        {
            resource_error(error, 2);
        }
    }
    std::array<u32, 12> parameters;
    std::memcpy(parameters.data(), resource.data() + preset * 0x30, 0x30);
    set_audio_listener(state, parameters);
    release_animation_resource(registry, resource);
    state.preset = preset;
}
}
