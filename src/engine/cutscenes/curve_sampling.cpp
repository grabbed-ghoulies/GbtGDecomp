#include "curve_sampling.h"
#include "../audio/listener_settings.h"
#include "gbtg/anim.h"

namespace gbtg
{
// Xbox 0x000512D0
void sample_cutscene_actor_position(const CutsceneActorPositionView* actors,
    std::int8_t index, float time, Vec3& output, i32 interpolate)
{
    const auto& channels = actors[index].position;
    if (interpolate != 0)
    {
        float first;
        float second;
        float fraction;
        fraction = sample_cutscene_curve_pair(channels[0], time, first, second);
        const float weighted_first_x = (1.f - fraction) * first;
        const float weighted_second_x = second * fraction;
        output.x = weighted_first_x + weighted_second_x;
        fraction = sample_cutscene_curve_pair(channels[1], time, first, second);
        const float weighted_first_y = (1.f - fraction) * first;
        const float weighted_second_y = second * fraction;
        output.y = weighted_first_y + weighted_second_y;
        fraction = sample_cutscene_curve_pair(channels[2], time, first, second);
        const float weighted_first_z = (1.f - fraction) * first;
        const float weighted_second_z = second * fraction;
        output.z = weighted_first_z + weighted_second_z;
        return;
    }
    {
        const auto& curve = channels[0];
        const long double scaled = (static_cast<long double>(time) / curve.duration)
            * static_cast<i32>(curve.count);
        u32 key = static_cast<u32>(truncate_runtime_float(scaled));
        if (curve.count <= key)
        {
            key = static_cast<u32>(curve.count) - 1;
        }
        output.x = curve.values[key];
    }
    {
        const auto& curve = channels[1];
        const long double scaled = (static_cast<long double>(time) / curve.duration)
            * static_cast<i32>(curve.count);
        u32 key = static_cast<u32>(truncate_runtime_float(scaled));
        if (curve.count <= key)
        {
            key = static_cast<u32>(curve.count) - 1;
        }
        output.y = curve.values[key];
    }
    {
        const auto& curve = channels[2];
        const long double scaled = (static_cast<long double>(time) / curve.duration)
            * static_cast<i32>(curve.count);
        u32 key = static_cast<u32>(truncate_runtime_float(scaled));
        if (curve.count <= key)
        {
            key = static_cast<u32>(curve.count) - 1;
        }
        output.z = curve.values[key];
    }
}

// Xbox 0x000514C0
float sample_cutscene_curve_pair(const CutsceneScalarCurveView& curve,
    float time, float& first, float& second)
{
    if (curve.count == 1)
    {
        first = curve.values[0];
        second = curve.values[0];
        return 0.f;
    }
    const float quotient = time / curve.duration;
    const float scaled = quotient * static_cast<float>(curve.count);
    if (scaled < 1.f)
    {
        first = curve.values[0];
        second = curve.values[1];
        return scaled;
    }
    const u32 index = static_cast<u32>(truncate_runtime_float(scaled));
    if (static_cast<u32>(curve.count) - 1 <= index)
    {
        first = curve.values[static_cast<u32>(curve.count) - 2];
        second = curve.values[static_cast<u32>(curve.count) - 1];
        return 1.f;
    }
    first = curve.values[index];
    second = curve.values[index + 1];
    return static_cast<float>(animation_remainder(scaled, 1.f));
}

// Xbox 0x00051430
void sample_cutscene_actor_scale(const CutsceneActorScaleView* actors,
    std::int8_t index, float time, Vec3& output)
{
    const auto& channels = actors[index].scale;
    {
        const auto& curve = channels[0];
        const long double scaled = (static_cast<long double>(time) / curve.duration)
            * static_cast<i32>(curve.count);
        u32 key = static_cast<u32>(truncate_runtime_float(scaled));
        if (curve.count <= key)
        {
            key = static_cast<u32>(curve.count) - 1;
        }
        output.x = curve.values[key];
    }
    {
        const auto& curve = channels[1];
        const long double scaled = (static_cast<long double>(time) / curve.duration)
            * static_cast<i32>(curve.count);
        u32 key = static_cast<u32>(truncate_runtime_float(scaled));
        if (curve.count <= key)
        {
            key = static_cast<u32>(curve.count) - 1;
        }
        output.y = curve.values[key];
    }
    {
        const auto& curve = channels[2];
        const long double scaled = (static_cast<long double>(time) / curve.duration)
            * static_cast<i32>(curve.count);
        u32 key = static_cast<u32>(truncate_runtime_float(scaled));
        if (curve.count <= key)
        {
            key = static_cast<u32>(curve.count) - 1;
        }
        output.z = curve.values[key];
    }
}
}
