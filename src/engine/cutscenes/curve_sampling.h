#pragma once
#include "gbtg/types.h"
#include <array>

namespace gbtg
{
struct CutsceneScalarCurveView
{
    u16 count; // +0
    float duration; // +4
    const float* values; // +8, relocated native pointer
};
struct CutsceneActorScaleView
{
    std::array<CutsceneScalarCurveView, 3> scale; // actor +40, +4C, +58
};
struct CutsceneActorPositionView
{
    std::array<CutsceneScalarCurveView, 3> position; // actor +1C, +28, +34
};
float sample_cutscene_curve_pair(const CutsceneScalarCurveView& curve,
    float time, float& first, float& second); // Xbox 0x000514C0
void sample_cutscene_actor_scale(const CutsceneActorScaleView* actors,
    std::int8_t index, float time, Vec3& output); // Xbox 0x00051430
void sample_cutscene_actor_position(const CutsceneActorPositionView* actors,
    std::int8_t index, float time, Vec3& output, i32 interpolate); // Xbox 0x000512D0
}
